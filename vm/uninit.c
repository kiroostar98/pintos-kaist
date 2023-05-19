/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type, // project 3 - anonymous page 과제 기준, load_segment()에서 vm_alloc_page_with_initializer()가 호출될 때 type인자가 VM_ANON이다. 이는 즉 uninit_new()때 type 매개변수로 VM_ANON이 들어온다는 것이다. 고로 현재는 UNINIT이지만 initialize될 때 ANON으로 바뀔 예정이다.
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault */
/* page fault handler: page_fault() -> vm_try_handle_fault()-> vm_do_claim_page() 
					-> swap_in() -> uninit_initialize()-> r각 타입에 맞는 initializer()와 vm_init() 호출 */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit; // 페이지 구조체 내 UNION 내 uninit struct를 통해, 기본적으로 구현되어 있었던 uninit_page에 접근.

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. 
	해당 페이지 타입에 맞도록 페이지를 초기화한다.
	만약 해당 페이지의 segment가 load되지 않은 상태면 lazy load해준다.
	init이 lazy_load_segmet일때에 해당.
	*/
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
	return;
}
