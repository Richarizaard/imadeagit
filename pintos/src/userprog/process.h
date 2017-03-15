#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "filesys/file.h"
#include "threads/thread.h"

#define MAX_ARGS 32

void process_init(void);
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool process_file_create(const char * filename, unsigned size);
bool process_file_remove(const char * filename);
int process_file_open(const char * filename);
int process_file_size(int fd);
int process_file_read(int fd, void *buffer, unsigned length);
int process_file_write(int fd, void * buffer, unsigned length);
void process_file_seek(int fd, unsigned position);
unsigned process_file_tell(int fd);
void process_file_close(int fd);
struct file_descriptor
{
	struct file * file;
	int fd;
	struct list_elem elem;
};


#endif /* userprog/process.h */
