#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static struct lock filesys_lock;

static thread_func start_process NO_RETURN;
static bool load (char *cmdline, void (**eip) (void), void **esp);
static size_t parse_args(char *command_str, char **args, size_t max_args);
static void file_descriptor_close(struct file_descriptor * desc);
static struct file_descriptor * get_file_descriptor(int fd);


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *commandStr) 
{
  char *fn_copy;
  char *another_copy;
  tid_t tid;

  printf("fn: %s\n", commandStr);
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  another_copy = palloc_get_page (0);
  if (another_copy == NULL)
  {
    palloc_free_page (fn_copy);
    return TID_ERROR;
  }
  strlcpy (fn_copy, commandStr, PGSIZE);
  strlcpy (another_copy, commandStr, PGSIZE);

  char * argv[MAX_ARGS];
  if (parse_args(another_copy, argv, 1) != 1)
  {
    palloc_free_page (fn_copy);
    palloc_free_page (another_copy);
    return TID_ERROR;
  }

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (another_copy, PRI_DEFAULT, start_process, fn_copy);
  palloc_free_page (another_copy);
  if (tid == TID_ERROR)
  {
    palloc_free_page (fn_copy); 
  }
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.
   
   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
	while(true);
  return -1;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  printf("%s: exit(%d)\n", cur->name, cur->tid);
  
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  
  struct list * list = &cur->file_list;
  struct list_elem * e;
  while ((e = list_begin(list)) != list_end (list)) 
  {
    file_descriptor_close(list_entry(e, struct file_descriptor, elem));
  }
  if (cur->exec_file != NULL)
    file_allow_write(cur->exec_file);
  if (pd != NULL) 
  {
    /* Correct ordering here is crucial.  We must set
       cur->pagedir to NULL before switching page directories,
       so that a timer interrupt can't switch back to the
       process page directory.  We must activate the base page
       directory before destroying the process's page
       directory, or our active page directory will be one
       that's been freed (and cleared). */
    cur->pagedir = NULL;
    pagedir_activate (NULL);
    pagedir_destroy (pd);
  }
}

bool
process_file_create(const char * filename, unsigned size)
{
  lock_acquire(&filesys_lock);
  bool success = filesys_create(filename, size);
  lock_release(&filesys_lock);
  
  return success;
}

bool
process_file_remove(const char * filename)
{
  lock_acquire(&filesys_lock);
	bool ret = filesys_remove(filename);
	lock_release(&filesys_lock);
  
  return ret;
}

int
process_file_open(const char * filename)
{
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

int
process_file_size(int fd)
{
  struct file_descriptor * desc = get_file_descriptor(fd);
  int size = -1;
    
  if (desc == NULL)
  {
    return -1;
  }
  
  lock_acquire(&filesys_lock);
  size = file_length(desc->file);
  lock_release(&filesys_lock);

	return size;
}

int
process_file_read(int fd, void *buffer, unsigned length)
{
  struct file_descriptor * desc = get_file_descriptor(fd);
  if (desc == NULL)
  {
    return -1;
  }
  
  lock_acquire(&filesys_lock);
  int bytesRead = file_read(desc->file, buffer, length);
  lock_release(&filesys_lock);
  
  return bytesRead;
}

int
process_file_write(int fd, void * buffer, unsigned length)
{
  struct file_descriptor * desc = get_file_descriptor(fd);
  
  lock_acquire(&filesys_lock);
  int bytesWritten = file_write(desc->file, buffer, length);
  lock_release(&filesys_lock);
  
  return bytesWritten;
}

void
process_file_seek(int fd, unsigned position)
{
  struct file_descriptor * desc = get_file_descriptor(fd);
  if (desc == NULL)
    return;
  
  lock_acquire(&filesys_lock);
  file_seek(desc->file, position);
  lock_release(&filesys_lock);
}

unsigned
process_file_tell(int fd)
{
  struct file_descriptor * desc = get_file_descriptor(fd);
  if (desc == NULL)
    return 0;
  
  lock_acquire(&filesys_lock);
  unsigned position = file_tell(desc->file);
  lock_release(&filesys_lock);
  return position;
}

void
process_file_close(int fd)
{
  struct file_descriptor * desc = get_file_descriptor(fd);
  if (desc == NULL)
    return;
 
  file_descriptor_close(desc);
}

static void file_descriptor_close(struct file_descriptor * desc)
{
  list_remove(&desc->elem);
  lock_acquire(&filesys_lock);
  file_close(desc->file);
  lock_release(&filesys_lock);
  free(desc);
}

void
process_init()
{
  lock_init(&filesys_lock);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, size_t argc, char **argv);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  char * argv[MAX_ARGS];
  size_t argc = parse_args(file_name, argv, MAX_ARGS);

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, argc, argv))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  if (success)
  {
    file_deny_write(file);
    t->exec_file = file;
  }
  else
  {
    file_close (file);
  }
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, size_t argc UNUSED, char **argv UNUSED)
{
  // TODO: Replace with our own work when we hate ourselves enough to write assembly
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
  {
    success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
    if (success)
    {
      *esp = PHYS_BASE;
      int i = argc;
      // this array holds reference to differences arguments in the stack
      uint32_t * arr[argc];
      while(--i >= 0)
      {
        *esp = *esp - (strlen(argv[i])+1)*sizeof(char);
        arr[i] = (uint32_t *)*esp;
        memcpy(*esp,argv[i],strlen(argv[i])+1);
      }
      *esp = *esp - 4;
      (*(int *)(*esp)) = 0;//sentinel
      i = argc;
      while( --i >= 0)
      {
        *esp = *esp - 4;//32bit
        (*(uint32_t **)(*esp)) = arr[i];
      }
      *esp = *esp - 4;
      (*(uintptr_t  **)(*esp)) = (*esp+4);
      *esp = *esp - 4;
      *(int *)(*esp) = argc;
      *esp = *esp - 4;
      (*(int *)(*esp))=0;
    }
    else
    {
      palloc_free_page (kpage);
    }
  }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

static struct file_descriptor * 
get_file_descriptor(int fd)
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

typedef enum
{
    NoArg,
    InArg,
    InQuotedArg,
    InEscapedArg,
} parser_states;

static size_t parse_args(char *command_str, char **args, size_t max_args)
{
  char * cur = command_str;
  size_t argCount = 0;
  parser_states state = NoArg;

  for (cur = command_str; *cur != '\0'; cur++)
  {
    switch(state)
    {
      case NoArg:
        if (argCount == max_args)
          return argCount; // We can't parse any more args
        switch (*cur)
        {
          case ' ':
            continue;
          case '"':
            state = InQuotedArg;
            args[argCount] = cur + 1;
            argCount++;
            continue;
          default:
            state = InArg;
            args[argCount] = cur;
            argCount++;
        }
        continue;
      case InArg:
        if (*cur == ' ')
        {
          state = NoArg;
          // End of the arg, null out this character
          *cur = '\0';
          continue;
        }
        continue;
      case InQuotedArg:
        if (*cur == '\\')
        {
          state = InEscapedArg;
          continue;
        }
        if (*cur == '"')
        {
          state = NoArg;
          // End of the arg, null out this character
          *cur = '\0';
        }
        continue;
      case InEscapedArg:
        state = InQuotedArg;
        if (*cur != '"' && *cur != '\\')
          continue;
        // We have an escaped character, copy everything to the right of this over one
        for (char *next = cur; *next != '\0'; next++)
        {
          *(next - 1) = *next;
        }
        cur--;
        continue;
      default:
        NOT_REACHED();
    }
  }
  
  return argCount;
}