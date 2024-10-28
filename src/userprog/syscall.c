#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "pagedir.h"
#include "threads/synch.h"
#include "threads/init.h"
#include "userprog/pagedir.h"

static void syscall_handler(struct intr_frame *f UNUSED);
static void check_pointer_valid(const void *p);
static void check_buffer_valid(const void *p, size_t size);
static char *copy_in_string(const char *us);
static void sys_exit(int status);
static int sys_practice(int i);
static pid_t sys_exec(const char *cmd_line);
static int sys_wait(pid_t pid);

void
syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED)
{
    uint32_t *args = ((uint32_t *) f->esp);  // Professor's code

    check_pointer_valid(args);  // Validate the pointer to the syscall number
    uint32_t syscall_number = args[0];

    switch (syscall_number)
    {
        case SYS_EXIT:
        {
            check_buffer_valid(&args[1], sizeof(args[1]));
            int status = (int) args[1];
            sys_exit(status);
            break;
        }
        case SYS_PRACTICE:
        {
          check_buffer_valid(&args[1], sizeof(args[1]));
          int i = (int) args[1];
          f->eax = sys_practice(i);
          break;
        }
        case SYS_HALT:
        {
            shutdown_power_off();
            break;
        }
        case SYS_EXEC:
        {
            const char *cmd_line;
            check_buffer_valid(&args[1], sizeof(args[1]));
            cmd_line = (const char *) args[1];
            // Validate that cmd_line points to a valid string
            char *kcmd_line = copy_in_string(cmd_line);
            if (kcmd_line == NULL) {
                f->eax = (pid_t) -1;
                break;
            }
            f->eax = sys_exec(kcmd_line);
            free(kcmd_line);
            break;
        }
        case SYS_WAIT:
        {
          pid_t pid;
            check_buffer_valid(&args[1], sizeof(args[1]));
            pid = (pid_t) args[1];
            f->eax = sys_wait(pid);
            break;
        }
        default:
          printf("Unknown syscall number: %d\n", syscall_number);
          sys_exit(-1);
    }
}

static void check_buffer_valid(const void *p, size_t size)
{
    if (p == NULL || !is_user_vaddr(p))
    {
      sys_exit(-1);
    }

    for (size_t i = 0; i < size; i++)
    {
        const uint8_t *addr = (const uint8_t *)p + i;
        if (!is_user_vaddr(addr) || pagedir_get_page(thread_current()->pagedir, addr) == NULL)
        {
          sys_exit(-1);
        }
    }
}

/* Validates that the given user pointer is valid */
static void check_pointer_valid(const void *p)
{
    if (p == NULL || !is_user_vaddr(p))
    {
        sys_exit(-1);
    }
    void *ptr = pagedir_get_page(thread_current()->pagedir, p);
    if (ptr == NULL)
    {
        sys_exit(-1);
    }
}

/* Copies a string from user space to kernel space */
static char *copy_in_string(const char *us)
{
    size_t max_length = 128;
    char *ks = malloc(max_length);
    if (ks == NULL) {
        sys_exit(-1);
    }

    size_t i = 0;
    while (i < max_length - 1) {  // Reserve space for null terminator
        check_pointer_valid((const void *)(us + i));
        ks[i] = us[i];
        if (ks[i] == '\0') {
            return ks;
        }
        i++;
    }

    // If the string exceeds max_length, terminate it and exit
    ks[max_length - 1] = '\0';
    sys_exit(-1);
    return NULL; 
}

/* Terminates the process with the given exit status */
static void sys_exit(int status)
{
  process_exit_with_status(status);
}

/* Implements the practice system call */
static int sys_practice(int i)
{
    return i + 1;
}

/* Implements the exec system call */
static pid_t sys_exec(const char *cmd_line)
{
    check_pointer_valid(cmd_line);
    char *kcmd_line = copy_in_string(cmd_line);
    if (kcmd_line == NULL)
    {
        return -1;
    }
    pid_t pid = process_execute(kcmd_line);
    free(kcmd_line);
    return pid;
}

/* Implements the wait system call */
static int sys_wait(pid_t pid)
{
    return process_wait(pid);
}