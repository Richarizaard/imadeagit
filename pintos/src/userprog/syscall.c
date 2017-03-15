#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/stdlib.h"

static void syscall_handler (struct intr_frame *);
static void syscall_halt(void);
static void syscall_exit(void);
static int syscall_open(void * arg1);
static int syscall_write(void * arg1);
static int syscall_read(void * arg1);
static void syscall_seek(void * arg1);
static unsigned syscall_tell(void * arg1);
static void syscall_close(void * arg1);
static uint32_t route_syscall(syscall_nums num, void * arg_start);
static bool check_user_pointer_validity(uint32_t *pd, const void * ptr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
  return process_execute(command_str);
}

static bool syscall_create(void * arg_start)
{
  char ** arg1 = (char **)arg_start;
  unsigned * arg2 = (unsigned *)(arg_start + sizeof(void *));

  char * filename = *arg1;
  unsigned size = *arg2;

  return process_file_create(filename, size);
}

static bool syscall_remove(void * arg_start)
{
	char ** arg1 = (char **)arg_start;
	char * filename = *arg1;
  
	return process_file_remove(filename);
}

static int syscall_open(void * arg_start)
{
	char ** arg1 = (char **)arg_start;
	char * filename = *arg1;

  return process_file_open(filename);
}

static int syscall_filesize(void * arg_start)
{
	int * arg1 = (int *)arg_start;
	int fd = *arg1;

  return process_file_size(fd);
}

static int syscall_read(void * arg_start)
{
  int * arg1 = (int *)arg_start;
  void ** arg2 = (void **)(arg_start + sizeof(void *));
  int * arg3 = (int *)(arg_start + sizeof(void *) * 2);

  int handle = *arg1;
  uint8_t *buffer = (uint8_t *) *arg2;
  unsigned length = *arg3;
  
  if (handle == 0)
  {
     for (unsigned i = 0; i < length; i++)
     {
       buffer[i] = input_getc();
     }
     return length;
  }
  return process_file_read(handle, buffer, length);
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

  switch(handle)
  {
    case 0:
         // Ignore
    break;
    case 1:
      return printf("%.*s", length, (char *)buffer);
    default:
      process_file_write(handle, buffer, length);
    break;
  }
  
  return 0;
}

static void syscall_seek(void * arg_start)
{
  int * arg1 = (int *)arg_start;
  unsigned * arg2 = (unsigned *)(arg_start + sizeof(void *));
  
  int fd = *arg1;
  unsigned position = *arg2;
  
  process_file_seek(fd, position);
}

static unsigned syscall_tell(void * arg_start)
{
  int * arg1 = (int *)arg_start;
  int fd = *arg1;
  
  return process_file_tell(fd);
}

static void syscall_close(void * arg_start)
{
	int * arg1 = (int *)arg_start;
	int fd = *arg1;
  
  process_file_close(fd);
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
    syscall_seek(arg_start);
    break;
  case SYS_TELL:
    ret = (uint32_t)syscall_tell(arg_start);
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