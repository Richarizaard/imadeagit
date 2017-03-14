#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdlib.h>

static void syscall_handler (struct intr_frame *);
static void syscall_halt(void);
static void syscall_exit(void);
static void syscall_close(void * arg1);
static int syscall_write(void * arg1);
static struct file_descriptor * get_file(int fd);
static uint32_t route_syscall(syscall_nums num, void * arg_start);
static bool check_user_pointer_validity(uint32_t *pd, const void * ptr);

static struct lock filesys_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  syscall_nums syscall_num = *(syscall_nums *)f->esp;
  void * arg1 = ((syscall_nums *)f->esp + 1);

  //printf("system call! sysnum: %d pointer: %p\n", syscall_num, f->esp);

  uint32_t ret = route_syscall(syscall_num, arg1);
  f->eax = ret;
}

/*
  Halts the entire system
*/
static void syscall_halt(void)
{
  shutdown_power_off();
}

/*
  Exit the current thread
*/
static void syscall_exit(void)
{
  thread_exit();
}

static tid_t syscall_exec(void * arg_start)
{
  char ** arg1 = (char **)arg_start;
  char * command_str = *arg1;
  return process_execute(arg1);
}

static bool syscall_create(void * arg_start)
{
  char ** arg1 = (char **)arg_start;
  unsigned * arg2 = (unsigned *)(arg_start + sizeof(void *));

  char * filename = *arg1;
  unsigned size = *arg2;

  lock_acquire(&filesys_lock);
  bool success = filesys_create(filename, size);
  lock_release(&filesys_lock);
  return success;
}

static bool syscall_remove(void * arg_start)
{
	char ** arg1 = (char **)arg_start;
	char * filename = *arg1;

	lock_acquire(&filesys_lock);
	bool ret = filesys_remove(filename);
	lock_release(&filesys_lock);
	return ret;
}

static int syscall_open(void * arg_start)
{
	char ** arg1 = (char **)arg_start;

	char * filename = *arg1;

	lock_acquire(&filesys_lock);
	struct file * file = filesys_open(filename);
	lock_release(&filesys_lock);
	if (file == NULL)
      return -1;

	struct thread * t = thread_current();
	int fd = t->next_fd++;
	if (fd < 2)
		return -1;

	struct file_descriptor * desc = malloc(sizeof(struct file_descriptor));
	if (desc == NULL)
		return -1;

	desc->file = file;
	desc->fd = fd;
	list_push_back(&t->file_list, &desc->elem);

	return fd;
}

static int syscall_filesize(void * arg_start)
{
	int * arg1 = (int *)arg_start;
	int fd = *arg1;
  int size = -1;
    
  struct file_descriptor * desc = get_file(fd);
  if (desc == NULL)
  {
    return -1;
  }
  
  lock_acquire(&filesys_lock);
  size = file_length(desc->file);
  lock_release(&filesys_lock);

	return size;
}

static int syscall_read(void * arg_start)
{
  int * arg1 = (int *)arg_start;
  void ** arg2 = (void **)(arg_start + sizeof(void *));
  int * arg3 = (int *)(arg_start + sizeof(void *) * 2);

  int handle = *arg1;
  uint8_t *buffer = (uint8_t *) *arg2;
  unsigned length = *arg3;
  
  if(handle == 0)
  {
     for(int i = 0; i < length; i++)
     {
       buffer[i] = input_getc();
     }
     return length;
  }
  
  struct file_descriptor * f = get_file(handle);
  if (f == NULL)
  {
    return -1;
  }
  int bytesRead = file_read(f->file, buffer, length);
  
  return bytesRead;
}

/*
  Writes to fd
*/
static int syscall_write(void * arg_start)
{
  int * arg1 = (int *)arg_start;
  void ** arg2 = (void **)(arg_start + sizeof(void *));
  int * arg3 = (int *)(arg_start + sizeof(void *) * 2);

  int handle = *arg1;
  void *buffer = *arg2;
  unsigned length = *arg3;

  struct file_descriptor * f;
  switch(handle)
  {
    case 0:
         // Ignore
    break;
    case 1:
      return printf("%.*s", length, buffer);
    default:
       f = get_file(handle);
       return file_write(f->file, buffer, length);
    break;
  }
  
  return 0;
}

static void syscall_close(void * arg_start)
{
	int * arg1 = (int *)arg_start;
	int fd = *arg1;
  
  struct file_descriptor * desc = get_file(fd);
  if (desc == NULL)
    return;
 
  
  file_close(desc->file);
  list_remove(&desc->elem);
  free(desc);
}

static struct file_descriptor * get_file(int fd)
{
	struct thread * t = thread_current();
  struct list * list = &t->file_list;
  struct list_elem * e = list_head (list);
  while ((e = list_next (e)) != list_end (list)) 
  {
    struct file_descriptor * desc = list_entry(e, struct file_descriptor, elem);
    if (desc->fd == fd)
    {
      return desc;
    }
  }
  return NULL;
}

static uint32_t route_syscall(syscall_nums num, void * arg_start)
{
  uint32_t ret = -999;
  switch (num)
  {
  case SYS_HALT:
    syscall_halt();
    break;
  case SYS_EXIT:
    syscall_exit();
    break;
  case SYS_EXEC:
    ret = syscall_exec(arg_start);
    break;
  case SYS_WAIT:
    break;
  case SYS_CREATE:
    ret = syscall_create(arg_start);
    break;
  case SYS_REMOVE:
	ret = syscall_remove(arg_start);
    break;
  case SYS_OPEN:
    ret = syscall_open(arg_start);
    break;
  case SYS_FILESIZE:
    ret = syscall_filesize(arg_start);
    break;
  case SYS_READ:
    ret = syscall_read(arg_start);
    break;
  case SYS_WRITE:
    ret = (uint32_t)syscall_write(arg_start);
    break;
  case SYS_SEEK:
    break;
  case SYS_TELL:
    break;
  case SYS_CLOSE:
    syscall_close(arg_start);
    break;

  case SYS_MMAP:
    break;
  case SYS_MUNMAP:
    break;

  case SYS_CHDIR:
    break;
  case SYS_MKDIR:
    break;
  case SYS_READDIR:
    break;
  case SYS_ISDIR:
    break;
  case SYS_INUMBER:
    break;
  default:
    break;
  }
  return ret;
}

/*
  Checks if the pointer is not a null pointer, not a pointer to unmapped virtual memory, and not a pointer to kernel virtual address space
*/
static bool
check_user_pointer_validity(uint32_t *pd, const void * ptr)
{
  if (ptr == NULL)
    return false;
  if (is_kernel_vaddr(ptr))
    return false;
  if (pagedir_get_page(pd, ptr) == NULL)
    return false;
  return true;
}