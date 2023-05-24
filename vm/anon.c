/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

// Gitbook 설명을 보면, anon page는 어떤 백업 공간도 갖고 있지 않다고 한다. 따라서 anon page를 swapping하는 것을 지원해주기 위해 swap disk라고 하는 임시 공간을 제공해준다. swap_disk는 디스크 내부에 있는 공간.
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	// swap_disk = NULL; // 기존 코드
	swap_disk = disk_get(1, 1); // 1:1 - swap

	// size_t swap_sector_size = disk_size(swap_disk) / SECTORS_PER_PAGE; 로 처음에 구현을 착수했으나, 이게 옳지 않은 로직이었던 것 같다.
    /* 
	1 sector = 512 bytes
	8 sectors = 1 page = 4096 bytes
	*/
    size_t swap_disk_size = disk_size(swap_disk);
    swap_table = bitmap_create(swap_disk_size); // '스왑디스크의 사이즈'개 만큼의 비트로 구성된 비트맵 생성. 스왑디스크의 한 섹터에 따른 비트 1개씩.
	if (swap_table == NULL){
		return;
	}
	bitmap_set_all(swap_table, false);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 
	page struct 안의 Union 영역은 현재 uninit page이다.
	ANON page를 초기화해주기 위해 해당 데이터를 모두 0으로 초기화해준다.
	Q. 이렇게 하면 Union 영역은 모두 다 0으로 초기화되나? -> 맞다. 
	*/
	// 기존에 있던 코드 삭제 ?!
	// struct uninit_page *uninit = &page->uninit;
	// memset(uninit, 0, sizeof(struct uninit_page));

	/* Set up the handler */
	page->operations = &anon_ops; // 초기화하려고 받은 페이지에 대한 operation을 anon_ops로 예약 
	struct anon_page *anon_page = &page->anon; // 초기화하려고 받은 페이지의 page 구조체의 union 중 anon_page에 struct anon_page *anon_page를 부여
	anon_page->swap_dst_sect_idx = -1; // 초기화~!

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *target_anon_page = &page->anon;

	disk_sector_t sector_no = target_anon_page->swap_dst_sect_idx;
	bitmap_scan_and_flip(swap_table, sector_no, SECTORS_PER_PAGE, true); // swap_table의 해당 swap sector를 free, 즉 sector의 비트를 다시 false로 바꿔주고, 해당 페이지의 PTE에서 present bit을 1으로 바꿔준다.

	for (int i=0; i<SECTORS_PER_PAGE; i++) {
		disk_read(swap_disk, sector_no + i, kva + DISK_SECTOR_SIZE*i); // page->frame->kva
	}

	pml4_set_page(thread_current()->pml4, page->va, kva, page->writable);

	target_anon_page->swap_dst_sect_idx = -1; // 초기화~!

    return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *target_anon_page = &page->anon; // evict할 페이지를 받아 그 page 구조체 안에 초기화해놨던 anon에 접근.

	disk_sector_t sector_no = bitmap_scan_and_flip(swap_table, 0, SECTORS_PER_PAGE, false); // 비트맵을 0부터, 즉 처음부터 순회한다. SECTORS_PER_PAGE(== 8 sectors == 1 page)개만큼 연속되게 false 값(== 해당 페이지의 PTE의 present bit가 0인 비트)을 갖는 비트의 묶음을 찾고, 찾는다면 그 묶음의 첫 시작이 되는 비트의 인덱스 sector_no를 반환한다.
	// scan and flip 즉, swap_table의 해당 페이지에 대한 swap slot의 비트를 true로 바꿔주고, 해당 페이지의 PTE에서 present bit을 0으로 바꿔준다. 이 시점부턴 프로세스가 이 페이지에 접근하면 page fault가 뜰 것이다!
    if (sector_no == BITMAP_ERROR) {
        return false;
    }

	for (int i=0; i<SECTORS_PER_PAGE; i++) {
		disk_write(swap_disk, sector_no + i, page->frame->kva + DISK_SECTOR_SIZE*i); // 한 페이지를 디스크에 써주기 위해 SECTORS_PER_PAGE(== 8)개의 섹터에 저장해야 한다.
		// 이때 디스크에 한 섹터마다 써준다. 하나의 페이지를 받아왔으므로 총 8번 써주게 된다. 각 섹터 크기의 DISK_SECTOR_SIZE(== 1 sector의 사이즈)만큼 frame의 물리주소를 이동해가며 frame의 물리주소에 있던 내용을 써준다.
	}

	target_anon_page->swap_dst_sect_idx = sector_no; // 작업되고 있는 anon_page의 구조체에 우리가 어떤 섹터로 이 녀석을 swap out했는지에 대한 인덱스 정보를 저장.

	pml4_clear_page(thread_current()->pml4, page->va); // 오답: page->frame
	palloc_free_page(page->frame->kva);
	// free(page->frame); // 왜 free를 해주지 않아야 작동이 되는 것인지는 아직도 이해가 안 갔다! 
	page->frame = NULL;

    return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
