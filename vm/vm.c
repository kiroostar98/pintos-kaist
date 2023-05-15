/* vm.c: Generic interface for virtual memory objects. */
#include "hash.h"
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "userprog/process.h"
#include "threads/mmu.h"

static bool
install_page(void *upage, void *kpage, bool writable)
{
	struct thread *t = thread_current();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page(t->pml4, upage) == NULL && pml4_set_page(t->pml4, upage, kpage, writable));
}

struct list frame_table;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
	struct list_elem *start = list_begin(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
static struct frame *vm_get_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* 페이지 초기화 흐름은 크게 세 가지로 볼 수 있다.
1. 커널이 새 페이지 request를 받으면 vm_alloc_page_with_initializer가 호출된다. 
이 initializer는 인자로 받은 해당 페이지 타입에 맞게 새 페이지를 초기화한 뒤 다시 유저 프로그램으로 제어권을 넘긴다.
2. 유저 프로그램이 실행되는 시점에서 page fault가 발생한다. 유저 프로그램 입장에서는 해당 페이지에 정보가 있다고 믿고 page에 접근을 시도할 테지만 이 페이지는 어떤 프레임과도 연결되지 않았기 때문.
3. fault를 다루는 동안, uninit_initialize가 발동하고 우리가 1에서 세팅해 둔 initializer를 호출한다. 
initializer는 각 페이지 유형에 맞게 anonymous page에는 anon_initializer를, file-backed page에는 file_backed_initializer를 호출할 것. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		// struct page *newpage = palloc_get_page(PAL_USER); // userpool에서 1 page를 alloc한다.
		struct page *newpage = malloc(sizeof(struct page));

		switch(VM_TYPE(type)) {
			case VM_ANON:
				uninit_new(newpage, upage, init, type, aux, anon_initializer);
				break;
			case VM_FILE:
				uninit_new(newpage, upage, init, type, aux, file_backed_initializer);
				break;
		}

		// page member 초기화
		newpage->writable = writable;
		bool succ = spt_insert_page (spt, newpage);

		/* TODO: Insert the page into the spt. */
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	// struct page *page = NULL;
	/* TODO: Fill this function. */
	struct page *page = (struct page *)malloc(sizeof(struct page));
	page->va = pg_round_down(va); //해당 va가 속해있는 페이지 시작 주소를 갖는 page를 만든다.

	/* e와 같은 해시값을 갖는 page를 spt에서 찾은 다음 해당 hash_elem을 리턴 */
	struct hash_elem *e = hash_find(&spt->spt_hash, &page->spt_hash_elem);
	free(page);
	return e != NULL ? hash_entry(e, struct page, spt_hash_elem): NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	if (!hash_insert(&spt->spt_hash, &page->spt_hash_elem)) {
		succ = false;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	// struct frame *frame = NULL;
	/* TODO: Fill this function. */
	// 위에 주석에서 palloc 하라고 했는데 palloc으로 하면 안되는 이유..?
	// frame->kva = palloc_get_page(PAL_USER);???
	// struct frame *frame = palloc_get_page(PAL_ZERO);????
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER); // 이걸 안 해줘서 계속 all faill 터졌다...

	if (frame->kva == NULL) {
		PANIC("todo");
	}

	list_push_back(&frame_table, &frame->frame_elem);
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	/*스택의 맨 밑(stack_bottom)보다 1 PAGE 아래에 페이지를 하나 만든다. 
	이 페이지의 타입은 ANON이어야 한다(스택은 anonymous page!) 
	맨 처음 UNINIT 페이지를 만들고 SPT에 넣은 뒤, 바로 claim한다.*/
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
// bool
// vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
// 		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
// 	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
// 	struct page *page = NULL;
// 	/* TODO: Validate the fault */
// 	/* TODO: Your code goes here */
// 	// 1. 유저가 참조하고자 하는 가상주소가 커널영역인지 확인, 그렇다면 바로 return false
// 	/* 2. 유저 스택 포인터 가져오는 법 => 이때 반드시 유저 스택 포인터여야 함! 
// 	모종의 이유로 인터럽트 프레임 내 rsp 주소가 커널 영역이라면 얘를 갖고 오는 게 아니라, 
// 	thread 내에 우리가 이전에 저장해뒀던 rsp_stack(유저 스택 포인터)를 가져온다.
// 	그게 아니라 유저 주소를 가리키고 있다면 f->rsp를 갖고 온다. */
// 	// 3. bogus인지 아닌지 확인. bogus가 아니라면 vm_do_claim_page() (페이지의 valid/invaild bit이 0이면, 즉 메모리 상에 존재하지 않으면 프레임에 메모리를 올리고 프레임과 페이지를 매핑시킨다.)
// 	//(현재, 우리가 할당해준 유저 스택의 맨 밑 주소값보다 더 아래에 접근하면 page fault가 발생한다. 
// 	//이 page fault가 stack growth에 관련된 것인지 먼저 확인하는 과정을 추가한다. 
// 	//맞다면>>>>>) vm_stack_growth()를 호출해 스택에 추가 페이지를 할당해 크기를 늘린다.
// 	return vm_do_claim_page (page);
// }

/* Return true on success */
/* page fault handler: page_fault() -> vm_try_handle_fault()-> vm_do_claim_page() 
					-> swap_in() -> uninit_initialize()-> r각 타입에 맞는 initializer()와 vm_init() 호출 */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	// Project 3
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	// 유효한 page fault인지를 가장 먼저 체크한다. "유효"하다는 것은 곧 유효하지 않은 접근 오류를 뜻한다. 만약 가짜 fault라면, 일부 내용을 페이지에 올리고 제어권을 사용자 프로그램에 반환한다.
	// fault가 뜬 주소와 대응하는 페이지 구조체를 해결한다. 이는 spt_find_page 함수를 통해 spt를 조사함으로써 이뤄진다.

	if(is_kernel_vaddr(addr) || addr == NULL){
        return false;
    }

	page = spt_find_page(spt, addr);

	if (!vm_do_claim_page(page)){
		return false;
	}

	return true;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if(install_page(page->va, frame->kva, page->writable)) { // put page to PT
		return swap_in(page, frame->kva); // mapping va and frame
	}
	return false;
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, spt_hash_elem);
	return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, spt_hash_elem);
  const struct page *b = hash_entry (b_, struct page, spt_hash_elem);

	return a->va < b->va;
}

bool page_insert(struct hash *h, struct page *p) {
    if(!hash_insert(h, &p->spt_hash_elem))
		return true;
	else
		return false;
}

bool page_delete(struct hash *h, struct page *p) {
	if(!hash_delete(h, &p->spt_hash_elem)) {
		return true;
	}
	else
		return false;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
