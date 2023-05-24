#ifndef THREADS_VADDR_H
#define THREADS_VADDR_H

#include <debug.h>
#include <stdint.h>
#include <stdbool.h>

#include "threads/loader.h"

/* Functions and macros for working with virtual addresses.
 *
 * See pte.h for functions and macros specifically for x86
 * hardware page tables. */

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* Page offset (bits 0:12). */
#define PGSHIFT 0                          /* Index of first offset bit. */
#define PGBITS  12                         /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)              /* Bytes in a page. */ // == 4096
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* Page offset bits (0:12). */

/* Offset within a page. */
// 아래의 두 매크로는 가상 주소의 특정 부분을 추출하거나 조작하는 유틸리티 기능을 제공합니다.
#define pg_ofs(va) ((uint64_t) (va) & PGMASK)
// 이 매크로는 가상 주소 va의 오프셋 부분을 추출하는 데 사용됩니다.
// PGMASK는 페이지 내 최대 오프셋 값을 나타내는 비트 마스크입니다.
#define pg_no(va) ((uint64_t) (va) >> PGBITS)
// 이 매크로는 가상 주소 va와 관련된 페이지 번호를 계산하는 데 사용됩니다.
// PGBITS는 페이지 번호를 나타내는 데 사용되는 비트 수를 나타냅니다.
// 오른쪽 이동 연산(>>)을 사용하여 가상 주소 va의 비트를 PGBITS 위치만큼 오른쪽으로 이동하여 오프셋을 나타내는 하위 비트를 효과적으로 버립니다.

/* Round up to nearest page boundary. */
#define pg_round_up(va) ((void *) (((uint64_t) (va) + PGSIZE - 1) & ~PGMASK))

/* Round down to nearest page boundary. */
#define pg_round_down(va) (void *) ((uint64_t) (va) & ~PGMASK)

/* Kernel virtual address start */
#define KERN_BASE LOADER_KERN_BASE

/* User stack start */
#define USER_STACK 0x47480000

/* Returns true if VADDR is a user virtual address. */
#define is_user_vaddr(vaddr) (!is_kernel_vaddr((vaddr)))

/* Returns true if VADDR is a kernel virtual address. */
#define is_kernel_vaddr(vaddr) ((uint64_t)(vaddr) >= KERN_BASE)

// FIXME: add checking
/* Returns kernel virtual address at which physical address PADDR
 *  is mapped. */
#define ptov(paddr) ((void *) (((uint64_t) paddr) + KERN_BASE))

/* Returns physical address at which kernel virtual address VADDR
 * is mapped. */
#define vtop(vaddr) \
({ \
	ASSERT(is_kernel_vaddr(vaddr)); \
	((uint64_t) (vaddr) - (uint64_t) KERN_BASE);\
})

#endif /* threads/vaddr.h */
