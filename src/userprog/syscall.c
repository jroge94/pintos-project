#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

static void syscall_handler(struct intr_frame *f);
void sys_exit(int status);
int sys_write(int fd, const void *buffer, unsigned size);
void validate_user_pointer(const void *p);
void validate_user_buffer(const void *buffer, size_t size);

void syscall_init(void) {
    // Register the interrupt handler for system calls
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Handles the system call based on the syscall number. */
static void syscall_handler(struct intr_frame *f) {
    uint32_t *args = (uint32_t *)f->esp;  // Pointer to user stack arguments

    // Validate the user stack pointer
    validate_user_pointer(args);

    // Determine the system call number
    int syscall_number = args[0];

    switch (syscall_number) {
        case SYS_EXIT:
            validate_user_pointer(args + 1);  // Validate exit status pointer
            int status = (int)args[1];        // Retrieve exit status
            sys_exit(status);
            break;

        case SYS_WRITE:
            validate_user_pointer(args + 1);  // Validate file descriptor
            validate_user_pointer(args + 2);  // Validate buffer pointer
            validate_user_pointer(args + 3);  // Validate size pointer
            int fd = (int)args[1];
            const void *buffer = (const void *)args[2];
            unsigned size = (unsigned)args[3];

            // Validate buffer contents
            validate_user_buffer(buffer, size);

            // Set return value in eax for `sys_write`
            f->eax = sys_write(fd, buffer, size);
            break;

        default:
            printf("Unknown syscall number: %d\n", syscall_number);
            sys_exit(-1);  // Exit with status -1 for unknown syscall
            break;
    }
}

/* Exits the current process with the specified status code. */
void sys_exit(int status) {
    struct thread *cur = thread_current();
    if (cur->pcb != NULL) {
        cur->pcb->exit_status = status;   // Update exit status in PCB
    }
    printf("%s: exit(%d)\n", cur->name, status);  // Log the exit status
    thread_exit();  // Terminate the current thread
}

/* Writes to a specified file descriptor (currently only supports STDOUT). */
int sys_write(int fd, const void *buffer, unsigned size) {
    if (fd == STDOUT_FILENO) {  // Write to STDOUT
        putbuf(buffer, size);   // Write buffer content to console
        return size;            // Return number of bytes written
    } else {
        return -1;              // Invalid file descriptor
    }
}

/* Validates a pointer is within the user address space and mapped. */
void validate_user_pointer(const void *p) {
    struct thread *t = thread_current();

    // Check if pointer is NULL or outside user address space
    if (p == NULL || !is_user_vaddr(p)) {
        sys_exit(-1);  // Invalid pointer, exit with status -1
    }

    // Check if pointer is mapped in the page directory
    if (pagedir_get_page(t->pcb->pagedir, p) == NULL) {
        sys_exit(-1);  // Unmapped pointer, exit with status -1
    }
}

/* Validates an entire buffer by checking each byte within the buffer. */
void validate_user_buffer(const void *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        validate_user_pointer((const char *)buffer + i);
    }
}
