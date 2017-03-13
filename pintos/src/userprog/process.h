#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "filesys/file.h"
#include "threads/thread.h"

#define MAX_ARGS 32

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct file_descriptor
{
	struct file * file;
	int fd;
	struct list_elem elem;
};

#endif /* userprog/process.h */
