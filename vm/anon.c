/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

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

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = NULL;
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* page struct 안의 Union 영역은 현재 uninit page이다.
	   ANON page를 초기화해주기 위해 해당 데이터를 모두 0으로 초기화해준다. 
	   Q. 이렇게 하면 Union 영역은 모두 다 0으로 초기화되나? -> 맞다. */

	/* Set up the handler */
	struct uninit_page *uninit = &page->uninit;
    memset(uninit, 0, sizeof(struct uninit_page));

	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	// 구현해야함(Lazy Loading for Executable)
	// 이 함수는 처음에 page→operations에 있는 anonymous page에 대한 handler를 셋업한다. 
	// 우리는 아마 anon_page에 있는 정보를 업데이트할 필요가 있을 것이다(현재는 빈 구조체임). 
	// 이 함수는 anonymous page에 대한 initializer로서 사용된다.(i.e. VM_ANON)
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
