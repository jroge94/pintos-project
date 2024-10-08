#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

static void syscall_handler(struct intr_frame *);
void sys_exit(int status);
int sys_write(int fd, const void *buffer, unsigned size);
void check_pointer_valid(const void *p);
void check_buffer_valid(const void *p, size_t size);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    uint32_t *args = (uint32_t *)f->esp;  // Get the stack pointer (user arguments)

    // Validate the stack pointer
    check_pointer_valid(args);

    // Get the system call number from the stack
    int syscall_number = args[0];

    switch (syscall_number) {
        case SYS_EXIT:
            check_pointer_valid(args + 1);  // Validate argument pointer
            int status = (int)args[1];  // Extract the exit status
            sys_exit(status);
            break;

        case SYS_WRITE:
            check_pointer_valid(args + 1);  // Validate fd
            check_pointer_valid(args + 2);  // Validate buffer
            check_pointer_valid(args + 3);  // Validate size
            int fd = (int)args[1];
            const void *buffer = (const void *)args[2];
            unsigned size = (unsigned)args[3];

            // Validate the buffer
            check_buffer_valid(buffer, size);

            // Call the sys_write function and set the return value in eax
            f->eax = sys_write(fd, buffer, size);
            break;

        default:
            printf("Unknown syscall number: %d\n", syscall_number);
            sys_exit(-1);
            break;
    }
}

/* System call implementations */

/* Exits the current process with the given status. */
void sys_exit(int status) {
    struct thread *cur = thread_current();
    if (cur->pcb != NULL) {
        cur->pcb->exit_status = status;   // Access exit_status in struct process
    }
    printf("%s: exit(%d)\n", cur->name, status);
    thread_exit();  // Exit the current thread
}

/* Writes to a file descriptor (STDOUT). */
int sys_write(int fd, const void *buffer, unsigned size) {
    if (fd == STDOUT_FILENO) {  // Write to the console
        putbuf(buffer, size);  // Output buffer to the console
        return size;  // Return the number of bytes written
    } else {
        return -1;
    }
}

/* Helper functions */
/* Credit Kaiser */
/* Validates if the pointer is within the user address space and mapped. */
void check_pointer_valid(const void *p) {
    struct thread *t = thread_current();

    if (p == NULL || !is_user_vaddr(p)) {
        sys_exit(-1);  // Invalid pointer, exit with status -1
    }

    // Access pagedir via the process control block (pcb)
    if (pagedir_get_page(t->pcb->pagedir, p) == NULL) {
        sys_exit(-1);  // Unmapped memory, exit with status -1
    }
}

/* Validates a buffer by checking each byte within the buffer. */
void check_buffer_valid(const void *p, size_t size) {
    for (size_t i = 0; i < size; i++) {
        check_pointer_valid((const char *)p + i);
    }
}
