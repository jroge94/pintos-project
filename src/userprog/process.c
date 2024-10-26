#include "userprog/process.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/flags.h"
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <stdio.h>
#include <string.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdlib.h>

/* ELF types */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf() */
#define PE32Wx PRIx32 /* Print Elf32_Word in hexadecimal */
#define PE32Ax PRIx32 /* Print Elf32_Addr in hexadecimal */
#define PE32Ox PRIx32 /* Print Elf32_Off in hexadecimal */
#define PE32Hx PRIx16 /* Print Elf32_Half in hexadecimal */

/* Executable header */
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

/* Program header */
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
#define PT_NULL 0           /* Ignore */
#define PT_LOAD 1           /* Loadable segment */
#define PT_DYNAMIC 2        /* Dynamic linking info */
#define PT_INTERP 3         /* Name of dynamic loader */
#define PT_NOTE 4           /* Auxiliary info */
#define PT_SHLIB 5          /* Reserved */
#define PT_PHDR 6           /* Program header table */
#define PT_STACK 0x6474e551 /* Stack segment */

/* Flags for p_flags */
#define PF_X 1 /* Executable */
#define PF_W 2 /* Writable */
#define PF_R 4 /* Readable */

#define MAX_ARGS 128

static struct semaphore temporary;
static thread_func start_process NO_RETURN;
static thread_func start_pthread NO_RETURN;
static bool load(const char* cmdline, void (**eip)(void), void** esp);
bool setup_thread(void (**eip)(void), void** esp);

/* Function declarations */
static bool setup_stack(void** esp);
static bool push_arguments(void** esp, char** argv, int argc);
static bool install_page(void* upage, void* kpage, bool writable);

/* Function prototypes for functions used before they're defined */
static bool validate_segment(const struct Elf32_Phdr* phdr, struct file* file);
static bool load_segment(struct file* file, off_t ofs, uint8_t* upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/* Struct to pass arguments to start_process */
struct process_args {
    char* file_name;
    struct thread* parent;
};


/* Initializes the user program system */
void userprog_init(void) {
  struct thread* t = thread_current();
  bool success;

  /* Allocate process control block
     It is imoprtant that this is a call to calloc and not malloc,
     so that t->pcb->pagedir is guaranteed to be NULL (the kernel's
     page directory) when t->pcb is assigned, because a timer interrupt
     can come at any time and activate our pagedir */
  t->pcb = calloc(sizeof(struct process), 1);
  success = t->pcb != NULL;

  /* Kill the kernel if we did not succeed */
  ASSERT(success);
}

/* Starts a new thread running a user program loaded from FILENAME */
pid_t process_execute(const char* cmd_line) {
    char* fn_copy;
    tid_t tid;
    char* save_ptr;

    /* Make a copy of CMD_LINE */
    fn_copy = palloc_get_page(0);
    if (fn_copy == NULL)
        return TID_ERROR;
    strlcpy(fn_copy, cmd_line, PGSIZE);

    /* Extract program name */
    char* file_name_copy = malloc(strlen(cmd_line) + 1);
    if (file_name_copy == NULL) {
        palloc_free_page(fn_copy);
        return TID_ERROR;
    }
    strlcpy(file_name_copy, cmd_line, strlen(cmd_line) + 1);
    char* prog_name = strtok_r(file_name_copy, " ", &save_ptr);

    /* Prepare arguments for start_process */
    struct process_args* p_args = malloc(sizeof(struct process_args));
    if (p_args == NULL) {
        palloc_free_page(fn_copy);
        free(file_name_copy);
        return TID_ERROR;
    }
    p_args->file_name = fn_copy;
    p_args->parent = thread_current();

    /* Create new thread to execute program */
    tid = thread_create(prog_name, PRI_DEFAULT, start_process, p_args);
    free(file_name_copy);
    if (tid == TID_ERROR) {
        palloc_free_page(fn_copy);
        free(p_args);
        return TID_ERROR;
    }

    /* Create child process entry */
    struct child_process* cp = malloc(sizeof(struct child_process));
    if (cp == NULL)
        return TID_ERROR;
    cp->pid = tid;
    cp->exit_status = -1; // Default exit status
    cp->wait_called = false;
    sema_init(&cp->sema, 0);

    /* Add to parent's child list */
    struct thread* parent = thread_current();
    list_push_back(&parent->child_list, &cp->elem);

    return tid;
}

/* Loads a user process and starts it running */
static void start_process(void* p_args_) {
    struct process_args* p_args = p_args_;
    char* file_name = p_args->file_name;
    struct thread* parent = p_args->parent;
    free(p_args); // Free the args struct

    struct intr_frame if_;
    bool success;
    char* token, *save_ptr;
    int argc = 0;
    char* argv[MAX_ARGS];

    /* Set parent pointer */
    struct thread* t = thread_current();
    t->parent = parent;
    list_init(&t->child_list);

    /* Tokenize the command line into arguments */
    for (token = strtok_r(file_name, " ", &save_ptr); token != NULL;
         token = strtok_r(NULL, " ", &save_ptr)) {
        argv[argc++] = token;
        if (argc >= MAX_ARGS)
            break;
    }

    /* Initialize interrupt frame and load executable */
    memset(&if_, 0, sizeof if_);
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;

    success = load(argv[0], &if_.eip, &if_.esp);

    /* Clean up and exit on failure */
    palloc_free_page(file_name);
    if (!success) {
        thread_exit();
    }

    /* Set up stack and arguments */
    success = setup_stack(&if_.esp) && push_arguments(&if_.esp, argv, argc);
    if (!success) {
        thread_exit();
    }

    /* Start the user process */
    asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(&if_) : "memory");
    NOT_REACHED();
}

int process_wait(pid_t child_pid UNUSED) {
  sema_down(&temporary);
  return 0;
}

/* Waits for a child process to die and returns its exit status */
// int process_wait(pid_t child_pid) {
//     struct thread* cur = thread_current();
//     struct list_elem* e;
//     struct child_process* cp = NULL;

//     /* Search for the child process */
//     for (e = list_begin(&cur->child_list); e != list_end(&cur->child_list); e = list_next(e)) {
//         cp = list_entry(e, struct child_process, elem);
//         if (cp->pid == child_pid) {
//             if (cp->wait_called) {
//                 return -1; // Already waited on
//             }
//             cp->wait_called = true;
//             break;
//         }
//     }

//     if (cp == NULL) {
//         return -1; // Child not found
//     }

//     /* Wait for child to exit */
//     sema_down(&cp->sema);

//     /* Retrieve exit status */
//     int status = cp->exit_status;

//     /* Remove child from list and free memory */
//     list_remove(&cp->elem);
//     free(cp);

//     return status;
// }

/* Free the current process's resources */
// void process_exit(void) {
//     struct thread* cur = thread_current();
//     uint32_t* pd;

//     /* Destroy the current process's page directory and switch back
//        to the kernel-only page directory. */
//     pd = cur->pagedir;
//     if (pd != NULL) {
//         cur->pagedir = NULL;
//         pagedir_activate(NULL);
//         pagedir_destroy(pd);
//     }

//     /* If this process has a parent, update its child_process struct */
//     if (cur->parent != NULL) {
//         struct list_elem* e;
//         for (e = list_begin(&cur->parent->child_list); e != list_end(&cur->parent->child_list); e = list_next(e)) {
//             struct child_process* cp = list_entry(e, struct child_process, elem);
//             if (cp->pid == cur->tid) {
//                 cp->exit_status = cur->exit_status;
//                 sema_up(&cp->sema);
//                 break;
//             }
//         }
//     }
// }

/* Free the current process's resources. */
void process_exit(void) {
  struct thread* cur = thread_current();
  uint32_t* pd;

  /* If this thread does not have a PCB, don't worry */
  if (cur->pcb == NULL) {
    thread_exit();
    NOT_REACHED();
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pcb->pagedir;
  if (pd != NULL) {
    /* Correct ordering here is crucial.  We must set
         cur->pcb->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
    cur->pcb->pagedir = NULL;
    pagedir_activate(NULL);
    pagedir_destroy(pd);
  }

  /* Free the PCB of this process and kill this thread
     Avoid race where PCB is freed before t->pcb is set to NULL
     If this happens, then an unfortuantely timed timer interrupt
     can try to activate the pagedir, but it is now freed memory */
  struct process* pcb_to_free = cur->pcb;
  cur->pcb = NULL;
  free(pcb_to_free);

  sema_up(&temporary);
  thread_exit();
}

/* Sets up the CPU for running user code in the current thread */
void process_activate(void) {
  struct thread* t = thread_current();

  /* Activate thread's page tables. */
  if (t->pcb != NULL && t->pcb->pagedir != NULL)
    pagedir_activate(t->pcb->pagedir);
  else
    pagedir_activate(NULL);

  /* Set thread's kernel stack for use in processing interrupts.
     This does nothing if this is not a user process. */
  tss_update();
}

/* Loads an ELF executable from FILE_NAME into the current thread */
static bool load(const char* file_name, void (**eip)(void), void** esp) {
  (void) esp;
  struct thread* t = thread_current();
  struct Elf32_Ehdr ehdr;
  struct file* file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory */
  t->pagedir = pagedir_create();
  if (t->pagedir == NULL)
      goto done;
  process_activate();

  /* Open executable file */
  file = filesys_open(file_name);
  if (file == NULL) {
      printf("load: %s: open failed\n", file_name);
      goto done;
  }

  /* Read and verify executable header */
  if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7)
      //|| ehdr.e_type != 2
      || ehdr.e_machine != 3
      //|| ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof(struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) {
      printf("load: %s: error loading executable\n", file_name);
      goto done;
  }

  /* Read program headers */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length(file))
          goto done;
      file_seek(file, file_ofs);

      if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
          goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) {
      case PT_NULL:
      case PT_NOTE:
      case PT_PHDR:
      case PT_STACK:
      default:
          /* Ignore this segment */
          break;
      case PT_DYNAMIC:
      case PT_INTERP:
      case PT_SHLIB:
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
                  zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes;
              } else {
                  /* Entirely zero */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
              }
              if (!load_segment(file, file_page, (void*)mem_page, read_bytes,
                                zero_bytes, writable))
                  goto done;
          } else
              goto done;
          break;
      }
  }

  /* Set the entry point */
  *eip = (void (*)(void))ehdr.e_entry;

  success = true;

done:
  /* Clean up */
  file_close(file);
  return success;
}

/* Checks whether PHDR describes a valid, loadable segment in FILE */
static bool validate_segment(const struct Elf32_Phdr* phdr, struct file* file) {
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

    /* The virtual memory region must be within the user address space range */
    if (!is_user_vaddr((void*)phdr->p_vaddr))
        return false;
    if (!is_user_vaddr((void*)(phdr->p_vaddr + phdr->p_memsz)))
        return false;

    /* The region cannot "wrap around" */
    if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
        return false;

    /* Disallow mapping page 0 */
    if (phdr->p_vaddr < PGSIZE)
        return false;

    /* It's okay */
    return true;
}

/* Loads a segment into memory */
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

/* Create a minimal stack by mapping a zeroed page at the top of user virtual memory */
static bool setup_stack(void** esp) {
    uint8_t* kpage;
    bool success = false;

    kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    if (kpage != NULL) {
        success = install_page(((uint8_t*)PHYS_BASE) - PGSIZE, kpage, true);
        if (success)
            *esp = PHYS_BASE - 4 ;
        else
            palloc_free_page(kpage);
    }
    return success;
}

/* Pushes program arguments onto the stack */
static bool push_arguments(void **esp, char **argv, int argc) {
    int i;
    char *arg_addresses[MAX_ARGS];

    // Push arguments onto the stack in reverse order
    for (i = argc - 1; i >= 0; i--) {
        *esp -= strlen(argv[i]) + 1;  // Move `esp` down by argument length + null terminator
        memcpy(*esp, argv[i], strlen(argv[i]) + 1);  // Copy argument to stack
        arg_addresses[i] = *esp;  // Store address of argument on the stack
    }

    // Word-align the stack pointer
    *esp = (void *)((uintptr_t)(*esp) & ~0x3);

    // Push a null pointer sentinel
    *esp -= sizeof(char *);
    memset(*esp, 0, sizeof(char *));

    // Push addresses of arguments
    for (i = argc - 1; i >= 0; i--) {
        *esp -= sizeof(char *);
        memcpy(*esp, &arg_addresses[i], sizeof(char *));
    }

    // Push argv (address of argv[0])
    char **argv_ptr = *esp;
    *esp -= sizeof(char **);
    memcpy(*esp, &argv_ptr, sizeof(char **));

    // Push argc
    *esp -= sizeof(int);
    memcpy(*esp, &argc, sizeof(int));

    // Push fake return address
    *esp -= sizeof(void *);
    memset(*esp, 0, sizeof(void *));

    return true;
}


/* Adds a mapping from user virtual address UPAGE to kernel virtual address KPAGE */
static bool install_page(void *upage, void *kpage, bool writable) {
    struct thread *t = thread_current();

    bool success = (pagedir_get_page(t->pagedir, upage) == NULL
                    && pagedir_set_page(t->pagedir, upage, kpage, writable));

    return success;
}


/* Returns true if t is the main thread of the process p */
bool is_main_thread(struct thread* t, struct process* p) {
  return p->main_thread == t;
}

/* Gets the PID of a process */
pid_t get_pid(struct process* p) { return (pid_t)p->main_thread->tid; }

/* Creates a new stack for the thread and sets up its arguments.
   Stores the thread's entry point into *EIP and its initial stack
   pointer into *ESP. Handles all cleanup if unsuccessful. Returns
   true if successful, false otherwise.

   This function will be implemented in Project 2: Multithreading. For
   now, it does nothing. You may find it necessary to change the
   function signature. */
// bool setup_thread(void (**eip)(void) UNUSED, void** esp UNUSED) {
//   return false;
// }

/* Starts a new thread with a new user stack running SF, which takes
   TF and ARG as arguments on its user stack. This new thread may be
   scheduled (and may even exit) before pthread_execute () returns.
   Returns the new thread's TID or TID_ERROR if the thread cannot
   be created properly.

   This function will be implemented in Project 2: Multithreading and
   should be similar to process_execute (). For now, it does nothing.
   */
// tid_t pthread_execute(stub_fun sf UNUSED, pthread_fun tf UNUSED,
//                       void* arg UNUSED) {
//   return -1;
// }

/* A thread function that creates a new user thread and starts it
   running. Responsible for adding itself to the list of threads in
   the PCB.

   This function will be implemented in Project 2: Multithreading and
   should be similar to start_process (). For now, it does nothing. */
//static void start_pthread(void* exec_ UNUSED) {}

/* Waits for thread with TID to die, if that thread was spawned
   in the same process and has not been waited on yet. Returns TID on
   success and returns TID_ERROR on failure immediately, without
   waiting.

   This function will be implemented in Project 2: Multithreading. For
   now, it does nothing. */
//tid_t pthread_join(tid_t tid UNUSED) { return -1; }

/* Free the current thread's resources. Most resources will
   be freed on thread_exit(), so all we have to do is deallocate the
   thread's userspace stack. Wake any waiters on this thread.

   The main thread should not use this function. See
   pthread_exit_main() below.

   This function will be implemented in Project 2: Multithreading. For
   now, it does nothing. */
//void pthread_exit(void) {}

/* Only to be used when the main thread explicitly calls pthread_exit.
   The main thread should wait on all threads in the process to
   terminate properly, before exiting itself. When it exits itself, it
   must terminate the process in addition to all necessary duties in
   pthread_exit.

   This function will be implemented in Project 2: Multithreading. For
   now, it does nothing. */
//void pthread_exit_main(void) {}
