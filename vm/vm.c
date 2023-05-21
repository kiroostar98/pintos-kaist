/* vm.c: Generic interface for virtual memory objects. */
#include "hash.h"
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "userprog/process.h"
#include "threads/mmu.h"
#define USER_STK_LIMIT (1 << 20)

static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
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
	lock_init(&kill_lock);
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
			return VM_TYPE (page->uninit.type); // project 3 - anonymous page 과제 기준, 무조건 VM_ANON이 담겨있는 것이다~
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
static struct frame *vm_get_frame (void);
void spt_destroy_func(struct hash_elem *e, void *aux);

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
		vm_initializer *init, void *aux) { // vm_initializer를 호출함으로써 page 하나를 만들어서 가상주소(va)를 만들어낸다.

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		
		struct page *newpage = calloc(1, sizeof(struct page));

		switch(VM_TYPE(type)) {
			case VM_ANON:
				uninit_new(newpage, upage, init, type, aux, &anon_initializer);
				break;
			case VM_FILE:
				uninit_new(newpage, upage, init, type, aux, &file_backed_initializer);
				break;
		}

		// page member 초기화
		newpage->writable = writable;
		// bool succ = spt_insert_page (spt, newpage);

		/* TODO: Insert the page into the spt. */
		return spt_insert_page (spt, newpage);
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
	int succ = true;
	/* TODO: Fill this function. */
	if (hash_insert(&spt->spt_hash, &page->spt_hash_elem) != NULL) {
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
	frame->kva = palloc_get_page(PAL_USER); // 이걸 안 해줘서 계속 all fail 터졌다...

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
	void *pg_addr = pg_round_down(addr);
	ASSERT((uintptr_t)USER_STACK - (uintptr_t)pg_addr <= (1 << 20));

	while (vm_alloc_page(VM_ANON, pg_addr, true)) {
		struct page *pg = spt_find_page(&thread_current()->spt, pg_addr);
		vm_claim_page(pg_addr);
		pg_addr += PGSIZE;
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
/* 
사용자 프로그램이 실행되면서 어느 시점에서, 프로그램이 소유한 것으로 생각되는 페이지에 액세스하려고 하지만 아직 내용이 없을 때 페이지 폴트가 발생.
페이지 폴트에서 페이지 폴트 핸들러(page_fault in userprog/exception.c)는 제어를 vm/vm.c의 vm_try_handle_fault으로 전달.
vm_try_handle_fault에서 먼저 이것이 유효한 페이지 폴트인지 확인. 유효하다는 것은 유효하지 않은 액세스 오류를 의미.
가짜 오류(bogus fault)인 경우, 일부 내용을 페이지에 로드하고 제어(control)를 사용자 프로그램에 반환(return).
*/
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, 
					bool write UNUSED, bool not_present UNUSED) {	 // user : mode bit가 user인지 kernel인지
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;

	struct thread *curr = thread_current();
	void *fault_addr = addr;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	/* 유효한 page fault인지를 가장 먼저 체크한다. 
	"유효"하다는 것은 곧 유효하지 않은 접근 오류를 뜻한다. 만약 가짜 fault라면, 일부 내용을 페이지에 올리고 제어권을 사용자 프로그램에 반환한다.
	fault가 뜬 주소와 대응하는 페이지 구조체를 해결한다. 이는 spt_find_page 함수를 통해 spt를 조사함으로써 이뤄진다. */

	if (is_kernel_vaddr (fault_addr)) {
		return false;
	}

	/* 이 함수에서는 Page Fault가 스택을 증가시켜야하는 경우에 해당하는지 아닌지를 확인한다.
	스택 증가로 Page Fault 예외를 처리할 수 있는지 확인한 경우, Page Fault가 발생한 주소로 vm_stack_growth를 호출한다.

	/* 유저 스택 포인터 가져오는 법 => 이때 반드시 유저 스택 포인터여야 함! 
	모종의 이유로 인터럽트 프레임 내 rsp 주소가 커널 영역이라면 얘를 갖고 오는 게 아니라, 
	thread 내에 우리가 이전에 저장해뒀던 rsp_stack(유저 스택 포인터)를 가져온다.
	그게 아니라 유저 주소를 가리키고 있다면 f->rsp를 갖고 온다.
	그리고, bogus인지 아닌지 확인. (현재, 우리가 할당해준 유저 스택의 맨 밑 주소값보다 더 아래에 접근하면 page fault가 발생한다. 
	이 page fault가 stack growth에 관련된 것인지 먼저 확인하는 과정을 추가한다.)
	bogus가 아니라면 vm_do_claim_page() (페이지의 valid/invaild bit이 0이면, 즉 메모리 상에 존재하지 않으면 프레임에 메모리를 올리고 프레임과 페이지를 매핑시킨다.) 

	Page Fault가 스택을 증가시키는 경우는 총 두 경우이다. */
	// 1. 만약 폴트가 사용자 모드에서 발생하고, fault_addr이 f->rsp-8과 같거나 f->rsp보다 작다면, 
	if (user && ((fault_addr == (f->rsp-8)) || (f->rsp < fault_addr))) {
		if (fault_addr >= (USER_STACK - PGSIZE * 250) && (fault_addr < USER_STACK)) {
		// 이어서 fault_addr이 특정 범위(USER_STACK - PGSIZE * 250에서 USER_STACK) 내에 있는지 확인. 
		// 만약 그렇다면 vm_stack_growth 함수를 호출하여 스택 확장을 처리하고 성공을 나타내는 true를 반환.
			vm_stack_growth(fault_addr);
			return true;
		}
	}
	// 2. 폴트가 사용자 모드가 아닌 경우, fault_addr이 curr->rsp_stack - 8과 같거나 curr->rsp_stack보다 작다면, 
	else if (!user && ((fault_addr == (curr->rsp_stack - 8)) || (curr->rsp_stack < fault_addr))) {
		// 코드는 이전과 동일한 범위 확인을 수행. fault_addr이 범위 내에 있는 경우 vm_stack_growth를 호출. 성공을 나타내는 true를 반환.
    	if (fault_addr >= (USER_STACK - PGSIZE * 250) && (fault_addr < USER_STACK)) {
			vm_stack_growth(fault_addr);
			return true;
		}
	}

	// spt에서 주소에 대한 page를 찾아 반환. 만약 NULL이라면 오류.
	page = spt_find_page(spt, pg_round_down(addr));
	if (page == NULL){
		return false;
	}

	// 찾은 페이지가 아직 프레임과 매핑되어있지 않다면 vm_do_claim_page().
	if (page->frame == NULL){
		/* The page has not been initialized yet */
		// obtain the base address of the page containing the faulted virtual address
		if (!vm_do_claim_page(page)) {
			return false;
		}
	}
	// vm_do_claim_page()이 성공적이었다면 즉 매핑을 완료했다면 반드시 페이지테이블(pml4)에 해당 주소가 담겨있어야!
	if (pml4_get_page(thread_current()->pml4, pg_round_down(addr)) == NULL) {
		return false;
	}

	if (!not_present)
		return false;

	return true; // page_fault()안에서 호출된 vm_try_handle_fault()는 성공적으로 수행을 완료하고 return true를 해줘야. 그래야 exception으로서의 page_fault()가 호출됐었던 그 위치로 돌아가게(return) 되니까~
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
	struct thread *t = thread_current();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if(install_page(page->va, frame->kva, true)) { // put page to PT
		return swap_in(page, frame->kva); 		   // mapping va and frame
	} else {
		frame->page = NULL;
		page->frame = NULL;
		return false;
	}
    // if (pml4_set_page(t->pml4, page->va, frame->kva, page->writable) == false) {
	// 	frame->page = NULL;
	// 	page->frame = NULL;
    //     return false;
	// }
    // return swap_in(page, frame->kva);
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
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {
	// 해시테이블을 순회하기 위해 필요한 구조체
	struct hash_iterator i;
	/* 1. SRC의 해시 테이블의 각 bucket 내 elem들을 모두 복사한다. */
	hash_first(&i, &src->spt_hash);
	while (hash_next(&i)) { // src의 각각의 페이지를 반복문을 통해 복사
		struct page *parent_page = hash_entry (hash_cur (&i), struct page, spt_hash_elem);   // 현재 해시 테이블의 element 리턴
		enum vm_type type = page_get_type(parent_page);		// 부모 페이지의 type
    	void *upage = parent_page->va;				    	// 부모 페이지의 가상 주소
      	bool writable = parent_page->writable;				// 부모 페이지의 쓰기 가능 여부
      	vm_initializer *init = parent_page->uninit.init;	// 부모의 초기화되지 않은 페이지들 할당 위해 
		
		void* aux = parent_page->uninit.aux;
		// struct load_segment_container *aux = (struct load_segment_container *)malloc(sizeof(struct load_segment_container));
		// memcpy(aux, parent_page->uninit.aux, sizeof(struct load_segment_container));
		/* 두 코드 모두 supplemental_page_table 구조체를 가지고 있으며, 해시 테이블을 복사하는 데 사용되는 hash_iterator를 선언합니다. 
		그러나 두 코드는 다른 작업을 수행합니다.
		위의 코드에서는 src 구조체의 해시 테이블에서 각 page 구조체를 반복적으로 탐색하고, 
		각 페이지의 가상 주소, 타입, 쓰기 가능 여부 등의 정보를 사용하여 새 페이지를 할당하고 dst 구조체의 해시 테이블에 추가합니다. 
		이 때, page 구조체의 uninit 필드를 사용하여 페이지를 초기화합니다.
		반면에 수정된 코드에서는 부모 페이지와 자식 페이지 간의 데이터 복사를 추가하였습니다. 
		즉, 위 코드와 달리 vm_claim_page() 함수를 호출하여 물리 메모리와 매핑된 페이지의 데이터를 복사하여 자식 페이지에 새로 할당합니다. 
		이는 부모 페이지와 자식 페이지가 동일한 물리 페이지를 공유하지 않고, 자식 페이지가 자체 물리 페이지를 가지게 됩니다. */

		// 부모 타입이 uninit인 경우
		if (parent_page->operations->type == VM_UNINIT) { 
			if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
				// 자식 프로세스의 유저 메모리에 UNINIT 페이지를 하나 만들고 SPT 삽입.
				return false;
		}
      	else {  // 즉, else문의 코드는 실제 부모의 물리메모리에 매핑되어있던 데이터는 없는 상태이다. 그래서 아래에서 memcpy로 부모의 데이터 또한 복사해 넣어준다.
        	if(!vm_alloc_page(type, upage, writable)) // type에 맞는 페이지 만들고 SPT 삽입.
				return false;
          	if(!vm_claim_page(upage))  // 바로 물리 메모리와 매핑하고 Initialize한다.
				return false;
			// 모든 페이지에 대응하는 물리 메모리 데이터를 부모로부터 memcpy
			struct page* child_page = spt_find_page(dst, upage);
			memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
		}
	}
	return true;
}

void spt_destroy_func(struct hash_elem *e, void *aux) {
    const struct page *p = hash_entry(e, struct page, spt_hash_elem);
    // vm_dealloc_page(p);
	destroy(p);
	free(p);
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt) {
    /* TODO: Destroy all the supplemental_page_table hold by thread */
    lock_acquire(&kill_lock);
	// struct hash_iterator i;
	// hash_first(&i, &spt->spt_hash);
	// while (hash_next(&i)) { 
	// 	struct page *parent_page = hash_entry (hash_cur (&i), struct page, spt_hash_elem); 
	// 	if (parent_page->operations->type == VM_FILE) {
	// 		do_munmap(parent_page->va);
	// 	}
	// }
	hash_clear(&spt->spt_hash, spt_destroy_func); //project 3 - anonymous page 
    lock_release(&kill_lock);
    /* TODO: writeback all the modified contents to the storage. */
}
