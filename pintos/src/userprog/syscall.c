#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  intr_dump_frame(f);
  uint32_t base = f->ebp + 0x24;
  
  int argc = *(int*)(base + 4);
  char **argv = *(char***)(base + 8);
  printf ("system call! %d %s %s \n", argc, argv[0], argv[1]);
  thread_exit ();
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