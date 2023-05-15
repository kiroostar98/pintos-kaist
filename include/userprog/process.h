#define VM
#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);
void argument_stack(char **parse, int count, void **rsp);
int process_add_file(struct file *f);
struct file *process_get_file(int fd);
void process_close_file(int fd);
struct thread *get_child_process(int pid);

// project 3
// 우리는 현재 lazy loading 방식을 취하고 있고 이는 파일 전체를 다 읽어오지 않는다. 
// 그때 그때 필요할 때만 읽어오는데, 그걸 위해서는 우리가 어떤 파일의 어떤 위치에서 읽어와야 할지 알아야 하고 그 정보가 container안에 들어가 있다.
struct container{
    struct file *file;
    int32_t offset;
    size_t page_read_bytes;
};

#endif /* userprog/process.h */
