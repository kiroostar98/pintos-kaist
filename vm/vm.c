/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "hash.h"
#include "include/userprog/process.h"
#include "threads/mmu.h"
#define USER_STK_LIMIT (1 << 20)

unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
/* frame 관리 테이블 */
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
	list_init(&frame_table);
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */

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

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	// Project 3
	// *** 함수의 역할 ***
	// 인자로 주어진 TYPE에 맞춘 Unintialized Page를 하나 만든다. 
	// 나중에 이 페이지에 대한 페이지 폴트가 떠서 초기화되기 전까지는 UNINIT 타입으로 존재한다.

	/* TODO: 페이지를 생성하고 VM 유형에 따라 초기화자를 가져옵니다,
		* TODO: 그런 다음 uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다
		* TODO: uninit_new를 호출한 후 필드를 수정해야 합니다. */
	/* TODO: 스크립트에 페이지를 삽입합니다. */
	if (spt_find_page (spt, upage) == NULL) {
		/* 함수 포인터를 사용하여 TYPE에 맞는 페이지 초기화 함수를 사용한다. */
		struct page *new_page = (struct page *)malloc(sizeof(struct page));
		typedef bool (*initializeFunc)(struct page*, enum vm_type, void *kva);
		initializeFunc initializer = NULL;

		void *va_rounded = pg_round_down(upage);

		switch(VM_TYPE(type)){
			case VM_ANON:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}

		uninit_new(new_page, va_rounded, init, type, aux, initializer);
		new_page->writable = writable;
		/* 새로 만든 UNINIT 페이지를 프로세스의 spt에 넣는다.(아직 물리 메모리랑 매핑이 된 것도 아니고 타입에 맞춰 초기화도 되지 않았다.) */
		spt_insert_page(spt, new_page);
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	// 인자로 받은 spt 내에서 va를 키로 전달해서 이를 갖는 page를 리턴한다.
	struct page *page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;

	/*
		hash_find(): hash_elem을 리턴해준다. 이로부터 우리는 해당 page를 찾을 수 있어.
		해당 spt의 hash 테이블 구조체를 인자로 넣자. 해당 해시 테이블에서 찾아야 하니까.
		근데 우리가 받은 건 va 뿐이다. 근데 hash_find()는 hash_elem을 인자로 받아야 하니
		dummy page 하나를 만들고 그것의 가상주소를 va로 만들어. 
		-> 그 다음 이 페이지의 hash_elem(=spt_hash_elem)을 넣는다.
	*/

	page->va = pg_round_down(va);
	e = hash_find(&spt->spt_hash, &page->spt_hash_elem);
	free(page);

	// hash_entry(HASH_ELEM, STRUCT, MEMBER): 매크로는 HASH_ELEM이 STRUCT 안에 포함된 MEMBER 
	// 멤버 변수로 링크되어 있을 때, 그 멤버변수의 값을 포함하는 STRUCT 구조체의 포인터를 반환하는 매크로
	return e != NULL ? hash_entry(e, struct page, spt_hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
	// 인자로 주어진 struct page를 spt에 삽입
	// hash_insert(): 
	// 새로운 데이터와 같은 데이터가 이미 해시 테이블에 존재하는 경우, 해당 데이터의 포인터가 반환 -> 삽입되지 않음
	// 같은 데이터가 해시 테이블에 존재하지 않는 경우, NULL 포인터가 반환 -> 새로운 데이터가 해시 테이블에 삽입되었음
	if (!hash_insert(&spt->spt_hash, &page->spt_hash_elem)){
		return true;
	}
	else{
		return false;
	}
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
	// 구현해야함
	// for문을 돌면서 pml4에서 제거할 애를 찾는다. 

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// 구현해야함
	// 이 함수는 page에 달려있는 frame 공간을 디스크로 내리는 swap out을 진행하는 함수이다. 
	// swap_out()은 매크로로 구현되어 있으니 이를 적용해주면 된다.
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	// 가상 메모리 프레임을 가져오는 함수
	// palloc()을 호출하고 프레임을 가져옵니다. 사용 가능한 페이지가 없으면 페이지를 퇴거하고 를 반환합니다. 
	// 이 함수는 항상 유효한 주소를 반환합니다. 즉, 사용자 풀 메모리가 가득 차면 이 함수는 프레임을 퇴거시켜 사용 가능한 메모리 공간을 확보합니다
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	// 사용 가능한 페이지를 얻습니다. 이 함수는 페이지 할당자(palloc)를 통해 사용자 영역(PAL_USER)에 대한 페이지를 할당
	frame->kva = palloc_get_page(PAL_USER);
	if (frame->kva == NULL) {
        PANIC("TODO");
    }

	list_push_back(&frame_table,&frame->frame_elem);
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	// anonymous페이지를 할당하여 스택 크기를 늘림
    vm_alloc_page_with_initializer(VM_ANON, pg_round_down(addr), 1, NULL, NULL);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	uintptr_t rsp;
	/* TODO: Validate the fault */
	/* if문으로 not present인지 확인 -> find page*/
	/* TODO: Your code goes here */
	if (not_present) {
		rsp = (user == true)? f->rsp : thread_current()->user_rsp;
		if (USER_STACK - USER_STK_LIMIT <= rsp - 8 && rsp - 8 <= addr && addr <= USER_STACK) {
			vm_stack_growth(addr);
		}
		
		page = spt_find_page(spt, addr);
		if (page == NULL)
			return false;

		if (write == 1 && page->writable == 0)
			return false;

		return vm_do_claim_page(page);
	}
	/*이 함수에서는 Page Fault가 스택을 증가시켜야하는 경우에 해당하는지 아닌지를 확인해야 합니다.
	스택 증가로 Page Fault 예외를 처리할 수 있는지 확인한 경우, 
	Page Fault가 발생한 주소로 vm_stack_growth를 호출합니다.*/
	/* rsp-8 <= addr <= user_stack이면  */

	return false;
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
	// 현재 스레드의 spt에서 va를 통해 page를 찾은뒤 null이면 false리턴
	struct page *page = NULL;
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
		return false;
	
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

    if (pml4_set_page(t->pml4, page->va, frame->kva, page->writable) == false)
        return false;

    return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	// 구현
	// 이 함수는 새로운 프로세스가 시작할 때 호출되거나 (initd in userprog/process.c )
	// 프로세스가 fork될 때 호출된다.(__do_fork in userprog.process.c)
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {
	// 해시테이블을 순회하기 위해 필요한 구조체
	struct hash_iterator i;
	/* 1. SRC의 해시 테이블의 각 bucket 내 elem들을 모두 복사한다. */
	hash_first(&i, &src->spt_hash);
	while (hash_next(&i)){ // src의 각각의 페이지를 반복문을 통해 복사
		struct page *parent_page = hash_entry (hash_cur (&i), struct page, spt_hash_elem);   // 현재 해시 테이블의 element 리턴
		enum vm_type type = page_get_type(parent_page);		// 부모 페이지의 type
    	void *upage = parent_page->va;				    		// 부모 페이지의 가상 주소
      	bool writable = parent_page->writable;				// 부모 페이지의 쓰기 가능 여부
      	vm_initializer *init = parent_page->uninit.init;	// 부모의 초기화되지 않은 페이지들 할당 위해 
      	void* aux = parent_page->uninit.aux;

		// 부모 타입이 uninit인 경우
      	if(parent_page->operations->type == VM_UNINIT) { 
        	if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
				// 자식 프로세스의 유저 메모리에 UNINIT 페이지를 하나 만들고 SPT 삽입.
            	return false;
      	}
      	else {  // 즉, else문의 코드는 실제 부모의 물리메모리에 매핑되어있던 데이터는 없는상태이다 그래서 아래에서 memcpy로 부모의 데이터 또한 복사해 넣어준다.
        	if(!vm_alloc_page(type, upage, writable)) // type에 맞는 페이지 만들고 SPT 삽입.
            	return false;
          	if(!vm_claim_page(upage))  // 바로 물리 메모리와 매핑하고 Initialize한다.
              	return false;
      	}

		// UNIT이 아닌 모든 페이지에 대응하는 물리 메모리 데이터를 부모로부터 memcpy
      	if (parent_page->operations->type != VM_UNINIT) { 
        	struct page* child_page = spt_find_page(dst, upage);
          	memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
      	}
  	}
  return true;	
}

static void spt_destroy_func(struct hash_elem *e, void *aux) {
    const struct page *pg = hash_entry(e, struct page, spt_hash_elem);
    vm_dealloc_page(pg);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// Supplemental Page Table이 소유하고 있던 모든 자원들을 다 free시킨다. 이 함수는 process_exit()에서 호출이 된다.
	// Supplemental Page Table 내의 모든 페이지 엔트리들을 방문해서 hash_destroy()를 수행한다. SPT 말고 PML4와 palloc된 물리 메모리는 이 함수 밖에서 프로세스가 없애준다.
	hash_destroy(&spt->spt_hash, spt_destroy_func);
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, spt_hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, spt_hash_elem);
  const struct page *b = hash_entry (b_, struct page, spt_hash_elem);

  return a->va < b->va;
}