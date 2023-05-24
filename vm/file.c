/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct load_segment_container *info = (struct load_segment_container *)page->uninit.aux;

	struct file_page *file_page = &page->file;
	file_page->file = info->file;
	file_page->file_ofs = info->ofs;
	file_page->page_read_bytes = info->page_read_bytes;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	if (page == NULL)
        PANIC("받아온페이지NULL");
	// struct file_page *file_page UNUSED = &page->file; // 기존 코드

	struct  load_segment_container *aux = (struct  load_segment_container *)page->uninit.aux;

	struct file *file = aux->file;
	off_t offset = aux->ofs;
	size_t page_read_bytes = aux->page_read_bytes;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	file_seek (file, offset);

	if(file_read(file, kva, page_read_bytes) != (int)page_read_bytes) {
		palloc_free_page(kva);
		return false;
	}

	memset(kva + page_read_bytes, 0, page_zero_bytes);

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	if (page == NULL)
        PANIC("받아온페이지NULL");
	// struct file_page *file_page UNUSED = &page->file; // 기존 코드

	struct load_segment_container* container = (struct load_segment_container *)page->uninit.aux;

	/* 수정된 페이지(더티 비트 1)는 파일에 업데이트 해 놓는다. 
		그리고 더티 비트를 0으로 만들어둔다. */
	if (pml4_is_dirty(thread_current()->pml4, page->va)){
		file_write_at(container->file, page->va, 
			container->page_read_bytes, container->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}

	/* present bit을 0으로 만든다. */
	pml4_clear_page(thread_current()->pml4, page->va);
	palloc_free_page(page->frame->kva);
	free(page->frame);
	page->frame = NULL;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file; // 기존 코드
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	struct file *get_file = file_reopen(file); // 한 file에 대한 동기화를 위해 file_open() 대신 file_reopen()을 사용.
	void * start_addr = addr; // 오프셋 시작 주소.
	/* fd로 열린 파일의 오프셋(offset) 바이트부터 length 바이트 만큼을 프로세스의 가상주소공간의 주소 addr에 매핑합니다.
	전체 파일은 addr에서 시작하는 연속 가상 페이지에 매핑됩니다. 
	파일 길이(length)가 PGSIZE의 배수가 아닌 경우 최종 매핑된 페이지의 일부 바이트가 파일 끝을 넘어 "stick out"됩니다. 
	page_fault가 발생하면 이 자투리 바이트를 전부 0으로 초기화하고, 페이지를 디스크에 다시 쓸 때 버립니다. */

	/* 주어진 파일 길이와 length를 비교해서 length보다 file 크기가 작으면 파일 통으로 싣고 파일 길이가 더 크면 주어진 length만큼만 load */
	size_t read_bytes = length > file_length(file) ? file_length(file) : length;
	size_t zero_bytes = read_bytes % PGSIZE == 0 ? 0 : PGSIZE - read_bytes % PGSIZE; // 마지막 페이지에 들어갈 자투리 바이트

	/* 파일을 페이지 단위로 잘라 해당 파일의 정보들을 container 구조체에 저장한다.
		FILE-BACKED 타입의 UINIT 페이지를 만들어 lazy_load_segment()를 vm_init으로 넣는다. */
	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct load_segment_container *container = (struct load_segment_container*)malloc(sizeof(struct load_segment_container));
		container->file = file_duplicate(get_file);
		container->ofs = offset;
		container->page_read_bytes = page_read_bytes;
		container->page_zero_bytes = page_zero_bytes;

		// 여기서는 페이지 할당을 FILE-BACKED로 해줘야 하니 아래 VM_FILE로 넣어준다.
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, container)) {
			// return NULL;
			return NULL;
		}

		//다음 페이지로 이동
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr       += PGSIZE;
		offset     += page_read_bytes;
	}
	// 최종적으로는 시작 주소를 반환
	return start_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	/* addr부터 연속된 모든 페이지 변경 사항을 업데이트하고 매핑 정보를 지운다.
	가상 페이지가 free되는 것이 아님. present bit을 0으로 만드는 것! */
	while (1) {
		struct page* page = spt_find_page(&thread_current()->spt, addr);

		if (page == NULL) {
			return;
		}

		struct load_segment_container *container = (struct load_segment_container *)page->uninit.aux;

		/* 수정된 페이지(dirty bit == 1)는 파일에 업데이트 해놓는다. 이후에 dirty bit을 0으로 만든다. */
		if (pml4_is_dirty(thread_current()->pml4, page->va)) {
			file_write_at(container->file, addr, container->page_read_bytes, container->ofs);
			pml4_set_dirty(thread_current()->pml4, page->va, 0);
		}
		pml4_clear_page(thread_current()->pml4, page->va);
		addr += PGSIZE;
    }
}
