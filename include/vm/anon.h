#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

/* anonymous page: 커널로부터 프로세스에게 할당된 일반적인 메모리 페이지. 파일과 매핑되지 않은 페이지. heap을 거치지 않고 할당받은 메모리 공간.
여기서 사실상 이 힙 역시도 anonymous page에 속한다고 보면 된다. 힙은 일종의 거대한 anonymous page의 집합으로 취급하면 될듯. heap 뿐만 아니라 stack을 사용할 때 역시 anonymous page를 할당받아서 쓴다.
고로, heap을 거치지 않고 할당받은 메모리 공간이라는 말보다, 엄밀히 말하면 힙도 하나의 anon page 집합이고 stack도 하나의 anon page 집합이며 
이외에는 anon page를 할당받는다고 하는 게 더 정확한 표현일 수 있다. */

/* 익명 페이지는 private or shared 방식으로 할당받을 수 있다. 
여기서 private/shared의 의미는 "서로 다른 프로세스 간에 페이지를 공유할 수 있는지"의 여부를 의미한다. 
우리가 위에서 언급한 각 프로세스별로 할당받는 힙과 스택은 private 방식으로 할당된 anonymous page이며, 
shared 방식은 프로세스 간 통신을 위해 사용되는 anonymous page이다. */

typedef bool vm_initializer (struct page *, void *aux);

struct anon_page {
	int swap_sec; // sector where swapped contents are stored.
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
