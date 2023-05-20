#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/flags.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "intrinsic.h"
#include "threads/synch.h"
#include "devices/input.h"
#include "lib/kernel/stdio.h"
#include "threads/palloc.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void check_address(void *addr);
void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file_name);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
tid_t fork(const char *thread_name, struct intr_frame *f);
int exec(const char *cmd_line);
int wait(int pid);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	lock_init(&filesys_lock);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	int syscall_n = f->R.rax; /* 시스템 콜 넘버 */
	switch (syscall_n)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi, f);
		break;
	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	case SYS_MMAP:
		f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap(f->R.rdi);
		break;
	}
}

void check_address(void *addr)
{
	if (addr == NULL)
		exit(-1);
	if (!is_user_vaddr(addr))
		exit(-1);
	// if (pml4_get_page(thread_current()->pml4, addr) == NULL)
	// 	exit(-1);
	return spt_find_page(&thread_current()->spt, addr);
}

void halt(void)
{
	power_off();
}

void exit(int status)
{
	struct thread *curr = thread_current();
	curr->exit_status = status; // 이거 wait에서 사용?
	printf("%s: exit(%d)\n", curr->name, status);
	thread_exit();
}

bool create(const char *file, unsigned initial_size)
{
	check_address(file);
	return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
	check_address(file);
	return filesys_remove(file);
}

int open(const char *file_name)
{
	check_address(file_name);
	struct file *file = filesys_open(file_name);
	if (file == NULL)
		return -1;
	int fd = process_add_file(file);
	if (fd == -1)
		file_close(file);
	return fd;
}

int filesize(int fd)
{
	struct file *file = process_get_file(fd);
	if (file == NULL)
		return -1;
	return file_length(file);
}

void seek(int fd, unsigned position)
{
	struct file *file = process_get_file(fd);
	if (file == NULL)
		return;
	file_seek(file, position);
}

unsigned tell(int fd)
{
	struct file *file = process_get_file(fd);
	if (file == NULL)
		return;
	return file_tell(file);
}

void close(int fd)
{
	struct file *file = process_get_file(fd);
	if (file == NULL)
		return;
	file_close(file);
	process_close_file(fd);
}

int read(int fd, void *buffer, unsigned size)
{
	check_address(buffer);

	struct page *page = spt_find_page(&thread_current()->spt, pg_round_down(buffer));
	if ((page != NULL) && (page->writable == 0)) {
		exit(-1);
	}

	int bytes_read = 0;
	char *ptr = (char *)buffer; // void주소값 buffer를 char포인터로 캐스팅.

	lock_acquire(&filesys_lock);
	if (fd == 0) {
		// 틀린 코드. 
		// for (int i = 0; i < size; i++)
		// {
		// 	*ptr++ = input_getc();
		// 	bytes_read++;
		// }
		// lock_release(&filesys_lock);
		char key;
		for (bytes_read = 0; bytes_read < size; bytes_read++)
		{
			key = input_getc();	  // 키보드에 한 개의 문자를 입력 받고,
			*ptr++ = key;		  // ptr에 받은 문자를 저장한다.
			if (key == '\n') {
				break;
			}
		}
	}
	else if (fd == 1) {
		lock_release(&filesys_lock);
		return -1;
	} 
	else {
		struct file *read_file = process_get_file(fd);
		if (read_file == NULL)
		{
			lock_release(&filesys_lock);
			return -1;
		}
		bytes_read = file_read(read_file, buffer, size);
	}
	lock_release(&filesys_lock);
	return bytes_read;
}

int write(int fd, const void *buffer, unsigned size)
{
	check_address(buffer);

	int bytes_write = 0;

	lock_acquire(&filesys_lock);
	if (fd == 1)
	{
		putbuf(buffer, size); // putbuf(): 버퍼 안에 들어있는 값 중 사이즈 N만큼을 console로 출력
		bytes_write = size;
	}
	else if (fd == 0) {
		lock_release(&filesys_lock);
		return -1;
	}
	else {
		struct file *fileobj = process_get_file(fd);
		if (fileobj == NULL) {
			lock_release(&filesys_lock);
			return -1;
		}
		bytes_write = file_write(fileobj, buffer, size);
	}
	lock_release(&filesys_lock);
	return bytes_write;
}

tid_t fork(const char *thread_name, struct intr_frame *f)
{
	return process_fork(thread_name, f);
}

int exec(const char *cmd_line)
{
	check_address(cmd_line);

	// process.c 파일의 process_create_initd 함수와 유사하다.
	// 단, 스레드를 새로 생성하는 건 fork에서 수행하므로
	// 이 함수에서는 새 스레드를 생성하지 않고 process_exec을 호출한다.

	// process_exec 함수 안에서 filename을 변경해야 하므로
	// 커널 메모리 공간에 cmd_line의 복사본을 만든다.
	// (현재는 const char* 형식이기 때문에 수정할 수 없다.)
	char *cmd_line_copy;
	cmd_line_copy = palloc_get_page(0);
	if (cmd_line_copy == NULL)
		exit(-1);							  // 메모리 할당 실패 시 status -1로 종료한다.
	strlcpy(cmd_line_copy, cmd_line, PGSIZE); // cmd_line을 복사한다.

	// 스레드의 이름을 변경하지 않고 바로 실행한다.
	if (process_exec(cmd_line_copy) == -1)
		exit(-1); // 실패 시 status -1로 종료한다.
}

int wait(int pid)
{
	return process_wait(pid);
}

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset) {
	if(fd == 0 || fd == 1){
		exit(-1);
	}

	if (spt_find_page(&thread_current()->spt, addr)) {
        return NULL;
	} // 기존 매핑된 페이지 집합(실행가능 파일이 동작하는 동안 매핑된 스택 또는 페이지를 포함)과 겹치는 경우 실패해야.
	// length로 몇 페이지가 필요한지 계산, 계산한 페이지 수 만큼 (addr + i)가 맵핑된 페이지인지 spt find로 확인.

	struct file *fileobj = process_get_file(fd);
	if(!fileobj || !filesize(fd) || file_length(fileobj) == 0){
		return NULL;
	}

	if (is_kernel_vaddr(addr) || addr == 0 || length == 0 || offset % PGSIZE != 0 || pg_round_down(addr) != addr || pg_ofs(addr) != 0  || KERN_BASE <= length) {
		return NULL;
	}

	return do_mmap(addr, length, writable, fileobj, offset);
}

void munmap (void *addr) {
	do_munmap(addr);
}