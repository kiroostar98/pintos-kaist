#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include "hash.h"

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */
	struct hash_elem spt_hash_elem;

	bool writable;

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;
/*  kva == pa + KERN_BASE이므로, 변환 관계 또한 간단합니다. 그래서 우리는 physical address를 다루기 위해 kva를 대신 사용합니다.
    kva는 항상 physical memory에 단순한 일대일 대응을 갖습니다.
    kva space는 곧 pa space이므로 유저가 저장한 정보도 항상 kva가 있습니다. */
	struct page *page;
	struct list_elem frame_elem;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
/* 페이지에는 스왑 인, 스왑 아웃 및 페이지 파괴와 같은 몇 가지 작업이 수행됩니다. 각 페이지 유형마다 이러한 작업에 대해 필요한 단계와 작업이 다릅니다. 
경우에 따라 VM_ANON 페이지와 VM_FILE 페이지에 대해 다른 destroy 함수를 호출해야 합니다.
한 가지 방법은 각 경우를 처리하기 위해 각 함수에서 switch-case 구문을 사용하는 것입니다. 
이를 처리하기 위해 객체 지향 프로그래밍의 "클래스 상속" 개념을 도입합니다. 
실제로 C 프로그래밍 언어에는 "클래스"나 "상속"이 없습니다. Linux와 같은 실제 운영 체제 코드에서 유사한 방식으로 개념을 실현하기 위해 "함수 포인터"를 사용합니다 .
함수 포인터는 여러분이 배운 다른 포인터와 마찬가지로 메모리 내의 함수 또는 실행 가능한 코드를 가리키는 포인터입니다. 
함수 포인터는 검사 없이 런타임 값을 기반으로 실행할 특정 함수를 호출하는 간단한 방법을 제공하기 때문에 유용합니다. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
struct supplemental_page_table {
	struct hash spt_hash;
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

struct load_segment_container {
	struct file *file;
    off_t ofs;
	uint8_t *upage;
    size_t page_read_bytes;
    size_t page_zero_bytes;
	bool writable;
};

struct lock kill_lock;

#endif  /* VM_VM_H */
