/* process.c - Implementation of user process management in Pintos */

#include "userprog/process.h"
#include "devices/input.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <string.h>

/* ELF types */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32 /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32 /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32 /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16 /* Print Elf32_Half in hexadecimal. */

/* ELF Executable Header */
struct Elf32_Ehdr {
  unsigned char e_ident[16];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};

/* ELF Program Header */
struct Elf32_Phdr {
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* Values for p_type */
#define PT_NULL 0           /* Ignore. */
#define PT_LOAD 1           /* Loadable segment. */
#define PT_DYNAMIC 2        /* Dynamic linking info. */
#define PT_INTERP 3         /* Name of dynamic loader. */
#define PT_NOTE 4           /* Auxiliary info. */
#define PT_SHLIB 5          /* Reserved. */
#define PT_PHDR 6           /* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

/* Flags for p_flags */
#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

#define MAX_ARGS 128

/* Function declarations */
static void start_process(void* file_name_) NO_RETURN;
static bool load(const char* cmdline, void (**eip)(void), void** esp,
                 char** argv, int argc);
static bool setup_stack(void** esp, char** argv, int argc);
static bool validate_segment(const struct Elf32_Phdr*, struct file*);
static bool load_segment(struct file*, off_t, uint8_t*, uint32_t, uint32_t,
                         bool);
static bool install_page(void* upage, void* kpage, bool writable);

/* Synchronization for file system operations */
static struct lock filesys_lock;

/* Initializes user programs in the system */
void userprog_init(void) {
  struct thread* t = thread_current();

  lock_init(&filesys_lock);

  /* Allocate process control block */
  t->pcb = calloc(1, sizeof(struct process));
  ASSERT(t->pcb != NULL);

  /* Initialize the list of child processes */
  list_init(&t->child_list);
  lock_init(&t->child_lock);
}

/* Starts a new thread running a user program loaded from FILE_NAME */
pid_t process_execute(const char* file_name) {
  char *fn_copy, *file_name_copy, *save_ptr;
  char* program_name;
  tid_t tid;

  /* Make a copy of FILE_NAME */
  fn_copy = palloc_get_page(0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy(fn_copy, file_name, PGSIZE);

  /* Make another copy for program_name extraction */
  file_name_copy = malloc(strlen(file_name) + 1);
  if (file_name_copy == NULL) {
    palloc_free_page(fn_copy);
    return TID_ERROR;
  }
  strlcpy(file_name_copy, file_name, PGSIZE);

  /* Extract program name */
  program_name = strtok_r(file_name_copy, " ", &save_ptr);

  /* Create a new thread to execute PROGRAM_NAME */
  tid = thread_create(program_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR) {
    palloc_free_page(fn_copy);
    free(file_name_copy);
    return TID_ERROR;
  }

  /* Add child process to the parent's child list */
  struct thread* child_thread = get_thread_by_tid(tid);
  struct child_process* cp = malloc(sizeof(struct child_process));
  if (cp == NULL) {
    palloc_free_page(fn_copy);
    free(file_name_copy);
    return TID_ERROR;
  }

  cp->pid = tid;
  cp->exit_status = -1;
  cp->waited = false;
  sema_init(&cp->sema_wait, 0);
  sema_init(&cp->load_sema, 0);

  child_thread->cp = cp;

  struct thread* cur = thread_current();
  lock_acquire(&cur->child_lock);
  list_push_back(&cur->child_list, &cp->elem);
  lock_release(&cur->child_lock);

  free(file_name_copy);

  /* Wait for the child process to load */
  sema_down(&cp->load_sema);

  /* Check if the child loaded successfully */
  if (!cp->load_success)
    return -1;

  return tid;
}

/* Parses the command line into argc and argv */
static char* parse_command_line(const char* cmdline, int* argc, char*** argv) {
  char* argv_storage[MAX_ARGS];
  char *token, *save_ptr;
  int count = 0;

  /* Make a writable copy of cmdline */
  char* cmdline_copy = palloc_get_page(0);
  if (cmdline_copy == NULL)
    PANIC("Not enough memory to parse command line.");
  strlcpy(cmdline_copy, cmdline, PGSIZE);

  /* Tokenize the command line */
  token = strtok_r(cmdline_copy, " ", &save_ptr);
  while (token != NULL && count < MAX_ARGS) {
    argv_storage[count++] = token;
    token = strtok_r(NULL, " ", &save_ptr);
  }

  /* Allocate memory for argv */
  *argv = malloc(sizeof(char*) * (count + 1));
  if (*argv == NULL)
    PANIC("Not enough memory for argv.");

  /* Initialize argv with tokens */
  for (int i = 0; i < count; i++) {
    (*argv)[i] = argv_storage[i];
  }
  (*argv)[count] = NULL; /* Null-terminate argv */

  *argc = count;

  return cmdline_copy;
}

/* Modified start_process function */
static void start_process(void* file_name_) {
  char* file_name = (char*)file_name_;
  struct thread* t = thread_current();
  struct intr_frame if_;
  bool success;
  int argc;
  char** argv;
  char* cmdline_copy = NULL;

  /* Parse command line into arguments */
  cmdline_copy = parse_command_line(file_name, &argc, &argv);

  /* Initialize interrupt frame and load executable */
  memset(&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /* Allocate process control block */
  t->pcb = calloc(1, sizeof(struct process));
  if (t->pcb == NULL)
    PANIC("Failed to allocate PCB.");

  /* Initialize the file descriptor table */
  t->fd_table_size = 128; /* Set the desired size for the fd table */
  t->fd_table = malloc(sizeof(struct file*) * t->fd_table_size);
  if (t->fd_table == NULL)
    PANIC("Failed to allocate file descriptor table.");
  memset(t->fd_table, 0, sizeof(struct file*) * t->fd_table_size);
  t->next_fd = 2; /* Typically, 0 is stdin, 1 is stdout; start from 2 */

  /* Load the executable */
  success = load(argv[0], &if_.eip, &if_.esp, argv, argc);

  /* Signal the parent process about load success */
  if (t->cp != NULL) {
    t->cp->load_success = success;
    sema_up(&t->cp->load_sema);
  }

  /* Clean up resources */
  palloc_free_page(file_name);
  if (argv != NULL)
    free(argv);
  if (cmdline_copy != NULL)
    palloc_free_page(cmdline_copy);

  /* If load failed, set exit status to -1 and exit. */
  if (!success) {
    thread_current()->exit_status = -1; // Set exit status
    thread_exit();                      // Terminate process if load fails
  }

  /* Start the user process by simulating a return from an interrupt */
  asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(&if_) : "memory");
  NOT_REACHED();
}

/* Waits for process with PID child_pid to die and returns its exit status */
int process_wait(pid_t child_pid) {
  struct thread* cur = thread_current();
  struct child_process* cp = NULL;
  struct list_elem* e;

  lock_acquire(&cur->child_lock);
  for (e = list_begin(&cur->child_list); e != list_end(&cur->child_list);
       e = list_next(e)) {
    struct child_process* child = list_entry(e, struct child_process, elem);
    if (child->pid == child_pid) {
      cp = child;
      break;
    }
  }

  if (cp == NULL) {
    lock_release(&cur->child_lock);
    return -1; /* Child not found */
  }

  if (cp->waited) {
    lock_release(&cur->child_lock);
    return -1; /* Already waited */
  }

  cp->waited = true;
  lock_release(&cur->child_lock);

  /* Wait for the child process to exit */
  sema_down(&cp->sema_wait);

  int status = cp->exit_status;

  /* Remove child process from the list and free memory */
  lock_acquire(&cur->child_lock);
  list_remove(&cp->elem);
  lock_release(&cur->child_lock);

  free(cp);

  return status;
}

void process_exit(void) {
  struct thread* cur = thread_current();
  uint32_t* pd;

  /* Print thread's exit status. */
  printf("%s: exit(%d)\n", cur->name, cur->exit_status);

  /* Close executable file and allow writes */
  if (cur->exec_file != NULL) {
    file_allow_write(cur->exec_file);
    file_close(cur->exec_file);
    cur->exec_file = NULL;
  }

  /* Notify parent process */
  if (cur->cp != NULL) {
    cur->cp->exit_status = cur->exit_status;
    sema_up(&cur->cp->sema_wait);
  }

  /* Clean up child processes */
  struct list_elem* e;
  lock_acquire(&cur->child_lock);
  while (!list_empty(&cur->child_list)) {
    e = list_pop_front(&cur->child_list);
    struct child_process* cp = list_entry(e, struct child_process, elem);
    free(cp);
  }
  lock_release(&cur->child_lock);

  /* Free the file descriptor table */
  if (cur->fd_table != NULL) {
    /* Close all open files */
    for (int i = 0; i < cur->fd_table_size; i++) {
      if (cur->fd_table[i] != NULL)
        file_close(cur->fd_table[i]);
    }
    free(cur->fd_table);
    cur->fd_table = NULL;
  }

  /* Destroy the current process's page directory */
  if (cur->pcb != NULL) {
    pd = cur->pcb->pagedir;
    if (pd != NULL) {
      cur->pcb->pagedir = NULL;
      pagedir_activate(NULL);
      pagedir_destroy(pd);
    }

    /* Free the PCB */
    free(cur->pcb);
    cur->pcb = NULL;
  }
}

/* Sets up the CPU for running user code in the current thread */
void process_activate(void) {
  struct thread* t = thread_current();

  /* Activate thread's page tables */
  if (t->pcb != NULL && t->pcb->pagedir != NULL)
    pagedir_activate(t->pcb->pagedir);
  else
    pagedir_activate(NULL);

  /* Set thread's kernel stack for use in processing interrupts */
  tss_update();
}

/* Loads an ELF executable from FILE_NAME into the current thread */
static bool load(const char* file_name, void (**eip)(void), void** esp,
                 char** argv, int argc) {
  struct thread* t = thread_current();
  struct Elf32_Ehdr ehdr;
  struct file* file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory */
  t->pcb->pagedir = pagedir_create();
  if (t->pcb->pagedir == NULL) {
    printf("load: %s: failed to create page directory\n", file_name);
    goto done;
  }
  process_activate();

  /* Open executable file */
  lock_acquire(&filesys_lock);
  file = filesys_open(file_name);
  if (file == NULL) {
    printf("load: %s: open failed\n", file_name);
    lock_release(&filesys_lock);
    goto done;
  }

  /* Deny write to the executable file */
  file_deny_write(file);
  lock_release(&filesys_lock);
  t->exec_file = file;

  /* Read and verify executable header */
  if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2
      || ehdr.e_machine != 3 || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof(struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) {
    printf("load: %s: error loading executable\n", file_name);
    goto done;
  }

  /* Read program headers */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) {
    struct Elf32_Phdr phdr;

    if (file_ofs < 0 || file_ofs > file_length(file)) {
      printf("load: %s: invalid file offset\n", file_name);
      goto done;
    }
    file_seek(file, file_ofs);

    if (file_read(file, &phdr, sizeof phdr) != sizeof phdr) {
      printf("load: %s: error reading program header\n", file_name);
      goto done;
    }
    file_ofs += sizeof phdr;

    switch (phdr.p_type) {
    case PT_NULL:
    case PT_NOTE:
    case PT_PHDR:
    case PT_STACK:
      /* Ignore this segment */
      break;
    case PT_DYNAMIC:
    case PT_INTERP:
    case PT_SHLIB:
      /* Invalid segment type */
      printf("load: %s: invalid segment type\n", file_name);
      goto done;
    case PT_LOAD:
      if (validate_segment(&phdr, file)) {
        bool writable = (phdr.p_flags & PF_W) != 0;
        uint32_t file_page = phdr.p_offset & ~PGMASK;
        uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
        uint32_t page_offset = phdr.p_vaddr & PGMASK;
        uint32_t read_bytes, zero_bytes;

        if (phdr.p_filesz > 0) {
          /* Normal segment */
          read_bytes = page_offset + phdr.p_filesz;
          zero_bytes
              = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes;
        } else {
          /* Entirely zero */
          read_bytes = 0;
          zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
        }

        if (!load_segment(file, file_page, (void*)mem_page, read_bytes,
                          zero_bytes, writable)) {
          goto done;
        }
      } else {
        printf("load: %s: invalid segment\n", file_name);
        goto done;
      }
      break;
    }
  }

  /* Set up stack */
  if (!setup_stack(esp, argv, argc)) {
    printf("load: %s: error setting up stack\n", file_name);
    goto done;
  }

  /* Start address */
  *eip = (void (*)(void))ehdr.e_entry;

  success = true;

done:
  /* If load failed, close the file */
  if (!success) {
    lock_acquire(&filesys_lock);
    if (file != NULL) {
      file_allow_write(file); // Balance file_deny_write()
      file_close(file);
      if (t->exec_file == file) {
        t->exec_file = NULL; // Prevent double close in process_exit()
      }
    }
    lock_release(&filesys_lock);
  }
  return success;
}

/* Checks whether PHDR describes a valid, loadable segment in FILE */
static bool validate_segment(const struct Elf32_Phdr* phdr,
                             struct file* file) {
  /* p_offset and p_vaddr must have the same page offset */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE */
  if (phdr->p_offset > (Elf32_Off)file_length(file))
    return false;

  /* p_memsz must be at least as big as p_filesz */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must be within user address space */
  if (!is_user_vaddr((void*)phdr->p_vaddr))
    return false;
  if (!is_user_vaddr((void*)(phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot wrap around across the kernel virtual address space */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0 */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  return true;
}

/* Loads a segment starting at offset OFS in FILE at address UPAGE */
static bool load_segment(struct file* file, off_t ofs, uint8_t* upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable) {
  ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT(pg_ofs(upage) == 0);
  ASSERT(ofs % PGSIZE == 0);

  file_seek(file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) {
    /* Calculate how to fill this page */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Get a page of memory */
    uint8_t* kpage = palloc_get_page(PAL_USER);
    if (kpage == NULL)
      return false;

    /* Load this page */
    if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes) {
      palloc_free_page(kpage);
      return false;
    }
    memset(kpage + page_read_bytes, 0, page_zero_bytes);

    /* Add the page to the process's address space */
    if (!install_page(upage, kpage, writable)) {
      palloc_free_page(kpage);
      return false;
    }

    /* Advance */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
  }
  return true;
}

/* Sets up the stack with command-line arguments */
static bool setup_stack(void** esp, char** argv, int argc) {
  uint8_t* kpage;
  bool success = false;
  int i;
  void* arg_addr[MAX_ARGS];

  /* Allocate a zeroed page at the top of user virtual memory */
  kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  if (kpage != NULL) {
    success = install_page(((uint8_t*)PHYS_BASE) - PGSIZE, kpage, true);
    if (success)
      *esp = PHYS_BASE; /* Start at the top of the page */
    else {
      palloc_free_page(kpage);
      return success;
    }
  } else
    return success;

  /* Push argument strings onto the stack in reverse order */
  for (i = argc - 1; i >= 0; i--) {
    size_t arg_len = strlen(argv[i]) + 1;
    *esp -= arg_len;
    memcpy(*esp, argv[i], arg_len);
    arg_addr[i] = *esp;
  }

  // /* Word-align the stack pointer */
  // *esp = (void*)((uintptr_t)(*esp) & 0xfffffffc);

  /* Word-align the stack pointer */
  *esp = (void*)((uintptr_t)(*esp) & ~3);

  /* Push a null pointer sentinel */
  *esp -= sizeof(char*);
  *(char**)(*esp) = NULL;

  /* Push the addresses of the arguments */
  for (i = argc - 1; i >= 0; i--) {
    *esp -= sizeof(char*);
    *(char**)(*esp) = arg_addr[i];
  }

  /* Save argv address */
  void* argv_addr = *esp;

  /* Push argv */
  *esp -= sizeof(char**);
  memcpy(*esp, &argv_addr, sizeof(char**));

  /* Push argc */
  *esp -= sizeof(int);
  memcpy(*esp, &argc, sizeof(int));

  /* Push fake return address */
  *esp -= sizeof(void*);
  *(void**)(*esp) = NULL;

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel virtual address
 * KPAGE */
static bool install_page(void* upage, void* kpage, bool writable) {
  struct thread* t = thread_current();

  /* Verify that there's not already a page at that virtual address */
  return (pagedir_get_page(t->pcb->pagedir, upage) == NULL
          && pagedir_set_page(t->pcb->pagedir, upage, kpage, writable));
}