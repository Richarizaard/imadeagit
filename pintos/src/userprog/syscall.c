#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static void syscall_halt();
static void syscall_exit();
static void syscall_write(void * arg1);
static void route_syscall(syscall_nums num, void * arg_start);
static bool check_user_pointer_validity(uint32_t *pd, const void * ptr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  intr_dump_frame(f);

  hex_dump(0, f->esp, 128, true);

  syscall_nums syscall_num = *(syscall_nums *)f->esp;
  void * arg1 = ((syscall_nums *)f->esp + 1);

  printf("system call! sysnum: %d pointer: %p\n", syscall_num, f->esp);

  route_syscall(syscall_num, arg1);

  thread_exit ();
}

/*
  Halts the entire system
*/
static void syscall_halt()
{
  shutdown_power_off();
}

/*
  Exit the current thread
*/
static void syscall_exit()
{
  thread_exit();
}

/*
  Writes to fd
*/
static void syscall_write(void * arg_start)
{
  int * arg1 = (int *)arg_start;
  void ** arg2 = (void **)((int *)arg1 + 1);
  int * arg3 = (int *)((void *)arg2 + 1);

  int handle = *arg1;
  void *buffer = *arg2;
  unsigned length = *arg3;
  printf("%s", buffer);
}

static void route_syscall(syscall_nums num, void * arg_start)
{
  switch (num)
  {
  case SYS_HALT:
    syscall_halt();
    break;
  case SYS_EXIT:
    syscall_exit();
    break;
  case SYS_EXEC:
    break;
  case SYS_WAIT:
    break;
  case SYS_CREATE:
    break;
  case SYS_REMOVE:
    break;
  case SYS_OPEN:
    break;
  case SYS_FILESIZE:
    break;
  case SYS_READ:
    break;
  case SYS_WRITE:
    syscall_write(arg_start);
    break;
  case SYS_SEEK:
    break;
  case SYS_TELL:
    break;
  case SYS_CLOSE:
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