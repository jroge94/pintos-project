---
has_copyright: false
nav_enabled: false
---

# Report Template

```markdown
# Project 1: User Programs

## Preliminaries

If you have any preliminary comments on your submission, notes for the TAs, please
give them here.

Please cite any offline or online sources you consulted while preparing your
submission, other than the PintOS documentation, course text, lecture notes, and
course staff.

## Argument Passing

### Argument Passing - Data Structures

- Copy here the declaration of each new or changed struct or struct member, global or
  static variable, typedef, or enumeration.  Identify the purpose of each in 25 words
  or less.

### Argument Passing - Algorithms

- Briefly describe how you implemented argument parsing.  How do you arrange for the elements of `argv[]` to be in the right order? How do you avoid overflowing the stack page?

### Argument Passing - Rationale

- Why does PintOS implement `strtok_r()` but not `strtok()`?
strtok_r() ids thread-safe. uses a third argument, a pointer to a char *, to store the parsing context, making it re-entrant and thread-safe.
- In PintOS, the kernel separates commands into a executable name and arguments. In Unix-like systems, the shell does this separation. Identify at least two advantages of the Unix approach.
The kernel only needs to execute the program with the provided arguments, without the overhead of parsing the command line itself.
Having the shell handle argument parsing provides users with the flexibility to customize their environment.

  

## System Calls

Provide the following four sections for each of the syscall tasks (process control,
file operations, and floating point operations).

### System Calls - Data Structures

- Copy here the declaration of each new or changed struct or struct member, global or
  static variable, typedef, or enumeration.  Identify the purpose of each in 25 words
  or less.

/* Structure to represent child processes. */
struct child_process {
    tid_t tid;                   // Thread ID of the child.
    int exit_status;             // Exit status of the child.
    bool wait_istrue;            // Whether wait() has been called on this child.
    struct semaphore sema_wait;  // Semaphore for parent to wait on child.
    struct list_elem elem;       // List element for child list.
};

/* In struct thread */
struct list child_list;          // List of child processes.
struct child_process *cp;        // Pointer to own child_process struct.
struct semaphore sema_exec;      // Semaphore for synchronization during exec
bool load_success;   
struct semaphore sema_exit;      // Semaphore for synchronization during exit


- Describe how file descriptors are associated with open files. Are file descriptors
  unique within the entire OS or just within a single process?

  File descriptors are unique only for a single process. Each process has a file descriptor table that is stored in the
  process control block, so for each process every file gets a unique file descriptor. These file descriptors are unique only for this single process.

### System Calls - Algorithms

/* New Functions */

bool validate_user_pointer(const void *ptr) {} // Check if user pointer is not NULL, below PHYS_BASE, and mapped in page directory

/* Copy data between user and kernel space. This is how we will retrieve the respective system call from the stack. */

bool copy_in(void *dst, const void *usrc, size_t size) 
bool copy_out(void *udst, const void *src, size_t size)


- Describe your code for reading and writing user data from the kernel.

To access user data from the kernel, we first validate user pointers before we perform any read or write operation. We implement a function called validate_user_pointer to check if the pointer is below PHYS_BASE and in the page directory. To read user data, copy data from the user space to the kernel with copy_in and use copy_out to write data to the user space.

- Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel.  What is the least and the greatest possible number of inspections of the page table (e.g. calls to `pagedir_get_page()`) that might result?  What about for a system call that only copies 2 bytes of data?  Is there room for improvement in these numbers, and how much?


- Briefly describe your implementation of the "wait" system call and how it interacts with process termination.

When the parent process calls wait(pid), it searches the child_list for the child_process struct corresponding to that pid. If the child process is still running, the parent process calls sema_down(&C->sema_wait) to wait until the child process exits. If the child process has already exited, it just returns the exit status.

- Any access to user program memory at a user-specified address can fail due to a
  bad pointer value.  Such accesses must cause the process to be terminated.  System
  calls are fraught with such accesses, e.g. a `write` system call requires reading
  the system call number from the user stack, then each of the call's three 
  arguments, then an arbitrary amount of user memory, and any of these can fail at 
  any point. 
  This poses a design and error-handling problem: how do you best avoid obscuring the
  primary function of code in a morass of error-handling?  Furthermore, when an error
  is detected, how do you ensure that all temporarily allocated resources (locks,
  buffers, etc.) are freed?  In a few paragraphs, describe the strategy or strategies
  you adopted for managing these issues. Give an example.

  To keep code organized, we can create helper functions. We plan to handle pointer validation and data copying this way. The function validate_user_pointer() will handle pointer validation, and copy_in() will handle data copying. 
  
  If any validation fails we ensure that the lock is released before exiting or returning an error to prevent resource leaks. Keeping the code organized as so can help manage the clutter that so much error handling might bring.

  
Implementation of practice syscall

1. If args[0] == SYS_PRACTICE
2. Validate user pointer with validate_user_pointer function
3. Safely copy the integer argument into kernel space from user space
4. Call the syscall implementation and set return value
    - f->eax = i + 1

Implementation of halt syscall
1. if args[0] = SYS_HALT
2. call function shutdown_power_off()

Implementation of wait sys call
Parent process
1. Traverse child_list to find the child_process with tid == pid
2. Check if child is already waited on
3. If not, set wait_istrue to true and call sema_down(&child->sema_wait) to block until child signals termination.
4. After unblocking, get exit status of child and remove it from child_list. Free its memory.
Child process on exit
1. In process_exit(), set cp->exit_status to the process's exit status.
2. Call sema_up(&cp->sema_wait) to unblock any waiting parent 
3. Cleanup resources

Implementation of exec System Call
Parent process
1. Initialize synchronization
  - Initialize sema_init and thread_current->load_success = false
2. Execute process_execute to create the child process
3. Call sema_down to wait for child to signal
4. Check load success with load_success is true/false
In Child process
1. Attempt to load exec
2. Set load status
3. Signal parent to unblock if successful

Implementation of exit system call
1. Set exit status 
2. Signal parent with sema_up to unblock waiting parent
3. Cleanup
  - Close all files
  - Release resources
  - Call thread_exit()

### System Calls - Synchronization

- Study the interface of the semaphore functions and describe the sequence of calls
  required for a thread to be able to wait for an event that happens in another
  thread.
  The thread we want to wait must first initializes a struct semaphore with a value of 0. To wait, call sema_down(&sema). This blocks the thread since the semaphore value is 0. After the event we are waiting for happens, the signaling thread will call sema_up(&sema) to increment the value stored and wake up the waiting thread.

- The `exec` system call returns `-1` if loading the new executable fails, so it
  cannot return before the new executable has completed loading.  How does your code
  ensure this?  How is the load success/failure status passed back to the thread that
  calls "exec"?

  In the implementation of the exec system call, we check load_success during the child process attempting to load the executable. If the exec loads correctly in the child process, this flag will return true and if not return -1. The parent process checks this flag after the child signals to it and ensures that exec does not return a child process that has not completed loading.

- Consider parent process P with child process C.  How do you ensure proper
  synchronization and avoid race conditions when P calls `wait(C)` before `C` exits? 
  After `C` exits?  How do you ensure that all resources are freed in each case?  How
  about when `P` terminates without waiting, before `C` exits?  After `C` exits?  Are
  there any special cases?

  P calls wait(C) before C exits

  P calls wait(C) after C exits

  P terminates without waiting, before C exits

  P terminates without waiting, after C exits

### System Calls - Rationale

- Why did you choose to implement access to user memory from the kernel in the way
  that you did?
- What advantages or disadvantages can you see to your design for file descriptors?
- The default `tid_t` to `pid_t` mapping is the identity mapping. If you changed it,
  what advantages are there to your approach?

## Changes

Discuss any changes you made since your initial design document. Explain why you
made those changes. Feel free to reiterate what you discussed with your TA during
the design review if necessary.

## Reflection

Discuss the contribution of each member. Make sure to be specific about the parts of
each task each member worked on. Reflect on the overall working environment and
discuss what went well and areas of improvement.

## Testing

For each of the 2 test cases you write, provide

- Description of the feature your test case is supposed to test.
- Overview of how the mechanics of your test case work, as well as a qualitative
  description of the expected output.
- Output and results of your own PintOS kernel when you run the test case. These
  files will have the extensions .output and .result.
- Two non-trivial potential kernel bugs and how they would have affected the output
  of this test case. Express these in the form "If my kernel did X instead of Y,
  then the test case would output Z instead". You should identify two different bugs
  per test case, but you can use the same bug for both of your two test cases. These
  bugs should be related to your test case (e.g. syntax errors don't)

In addition, tell us about your experience writing tests for PintOS. What can be
improved about the PintOS testing system? What did you learn from writing test cases?
```
