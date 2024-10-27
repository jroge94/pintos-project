/* syscall.c */

#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"

/* Additional includes for memory validation */
#include "threads/malloc.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool
is_valid_user_ptr(const void *ptr, size_t size) {
    if (!is_user_vaddr(ptr))
        return false;
    // Additional checks can be added here (e.g., page presence)
    return true;
}

static void syscall_handler(struct intr_frame* f) {
    // Ensure the stack pointer is valid
    if (!is_user_vaddr(f->esp)) {
        thread_exit(); // Or handle the error appropriately
    }

    uint32_t* args = (uint32_t*)f->esp;
    uint32_t syscall_number = args[0];

    switch (syscall_number) {
        case SYS_EXIT: {
            if (!is_user_vaddr(args + 1)) {
                thread_exit();
            }
            int status = args[1];
            printf("%s: exit(%d)\n", thread_current()->pcb->process_name, status);
            process_exit();
            break;
        }
        case SYS_WRITE: {
            /* SYS_WRITE: write(int fd, const void *buffer, unsigned size); */

            // Validate pointers
            if (!is_user_vaddr(args + 1) || !is_user_vaddr(args + 2) || !is_user_vaddr(args + 3)) {
                thread_exit();
            }

            int fd = (int)args[1];
            const void* buffer = (const void*)args[2];
            unsigned size = (unsigned)args[3];

            // Validate buffer
            if (!is_user_vaddr(buffer)) {
                thread_exit();
            }

            // Handle writing to standard output (fd = 1)
            if (fd == 1) {
                putbuf(buffer, size);
                f->eax = size; // Return number of bytes written
            } else {
                // Handle other file descriptors if implemented
                // For simplicity, return -1 for unsupported fds
                f->eax = -1;
            }
            break;
        }
        default:
    
            f->eax = -1;
            break;
    }
}
