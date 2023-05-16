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
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault */
static bool
uninit_initialize (struct page *page, void *kva) {
	// *** 함수의 역할 ***
	// UNINIT 페이지의 멤버를 초기화해줌으로써 페이지 타입을 인자로 주어진 
	// 타입(ANON, FILE, PAGE_CACHE)으로 변환시켜준다. 그리고 해당 segment가 load되지 않은 상태면 lazy load segment도 진행한다. 
	struct uninit_page *uninit = &page->uninit;

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	// 필요에 따라 vm/anon.c에 있는 vm_anon_init이나 anon_initializer를 수정할 수 있다.
	// 구현해야함(Lazy Loading for Executable)
	/*
		page fault handler는 콜 체인을 따르는데 swap_in을 호출하면 최종적으로 uninit_initialize에 도달한다. 
		이미 완성된 구현을 제공한다. 허나, 우리는 우리의 설계에 맞게 uninit_initialize를 수정해야 할 필요가 있다.
		첫 fault가 난 페이지를 초기화한다. 템플릿 코드는 처음에 vm_initializer와 aux를 수정한다. 
		그 다음 함수 포인터를 통해 대응하는 page_initializer를 호출한다. 우리는 아마 이 함수를 우리 설계에 맞게 수정해야 할 필요가 있을 것이다.
		...
		해당 페이지 타입에 맞도록 페이지를 초기화한다.
		만약 해당 페이지의 segment가 load되지 않은 상태면 lazy load해준다.
		init이 lazy_load_segmet일때에 해당. 
	*/
	return uninit->page_initializer (page, uninit->type, kva) && (init ? init (page, aux) : true);
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
}
