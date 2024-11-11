# Design Document

### Argument Passing and Checkpoint 1
#### Joel Rogers

## Data Structures and Functions

```c
process.h
/* Maximum number of arguments supported */
#define MAX_ARGS 128

process.c
pid_t process_execute(const char* cmd_line)
process_execute() // Extracts the program name and passes it to `thread_create()`.
start_process() //Parses the command line into `argc` and `argv`.
setup_stack() //Pushes arguments onto the stack.

syscall.c
void sys_exit(int status);/* Exits the current process with the given status. */
int sys_write(int fd, const void *buffer, unsigned size); /* Writes to a file descriptor (STDOUT). */
```

## Implementation Plan

###  Write Syscall Implementation

####  Modify `syscall.c` to Handle `SYS_WRITE`

* **Add `SYS_WRITE` Handling**:
  * In `syscall_handler`, add a case for `SYS_WRITE`.
  * Extract the file descriptor, buffer, and size from the user stack.
  * Validate the user pointers using `is_user_vaddr` and `pagedir_get_page`.

####  Implement `handle_write` Function

* **Write to `STDOUT_FILENO`**:
  * Check if the file descriptor is `STDOUT_FILENO` (1).
  * Use `putbuf` to write the buffer to the console.
  * Return the number of bytes written.
* **Handle Invalid File Descriptors**:
  * For other file descriptors, return `-1` or handle as needed.

####  Validate User Pointers

* **Prevent Kernel Crashes**:
  * Ensure that the buffer pointer provided by the user is valid.
  * Terminate the process with `exit(-1)` if the pointer is invalid.

####  Testing

* **Compile and Run Tests**:
  * Use existing tests like `stack-align-1` to verify functionality.
  * Write a user program that calls `write` to print to the console.

####  Handle `SYS_PRACTICE` in `syscall.c`
* **Add Case in `syscall_handler`**:
  * Extract the integer argument from the user stack.
  * Return the argument incremented by one.

#### **Argument Passing**

   * Implement argument parsing and passing for new processes started via `process_execute`.
   * Split the command string passed to `process_execute()` into arguments (`argc` and `argv`).
   * Update the `start_process` function to properly set up the stack for argument passing.
   * Ensure that the main function of the user program receives the correct arguments (`argc`, `argv[]`), where:
     * `argc` is the number of arguments.
     * `argv` is an array of pointers to the individual arguments.
   * Update `process_execute` and `start_process` to handle and pass command-line arguments correctly.

#### Stack Setup for User Processes

* Properly place `argc`, `argv`, and the actual arguments on the user process's stack.
* Ensure that the stack is correctly aligned
  
## Algorithms

#### Command-Line Argument Parsing

* **Tokenize the Command-Line String**:
  * Use `strtok_r` to split the command-line string.
  * Store tokens in `argv[]`.
* **Extract Program Name**:
  * `argv[0]` is the program name.
* **Limit Arguments**:
  * Ensure `argc` does not exceed `MAX_ARGS`.

#### Stack Setup for User Process

#### Push Arguments onto the Stack in Reverse Order

* **Iterate Backwards**:
  * Start from `argc - 1` down to `0`.
* **Copy Argument Strings**:
  * Adjust `*esp` and use `memcpy` to copy strings.
* **Store Addresses**:
  * Save the address of each argument in `arg_addresses[]`.

#### Word Align the Stack Pointer

* **Calculate Alignment**:
  * Use `(uintptr_t)(*esp) & 0xfffffffc`.
* **Adjust `*esp`**:
  * Decrease `*esp` to ensure alignment.

#### Push Null Pointer

* **Push `NULL`**:
  * Decrease `*esp` by `sizeof(char *)`.
  * Set the value at `*esp` to `NULL`.

#### Push Addresses of Arguments (`argv`)

* **Push Addresses**:
  * Loop from `argc - 1` down to `0`.
  * Decrease `*esp` and store `arg_addresses[i]`.

#### Push `argv` and `argc`

* **Push `argv` Pointer**:
  * Store the current `*esp` (which points to `argv[0]`).
* **Push `argc`**:
  * Decrease `*esp` by `sizeof(int)` and store `argc`.

#### Push Fake Return Address

* **Push `NULL`**:
  * Decrease `*esp` by `sizeof(void *)` and set to `NULL`.

#### Load the Executable

* **Use `load` Function**:
  * Call `load(argv[0], &if_.eip, &if_.esp)`.
* **Set Entry Point**:
  * `if_.eip` is set by `load`.
* **Handle Load Failure**:
  * If `load` returns `false`, terminate the process.

## Concept Check

### 1 `sc-bad-arg.c`

* **Purpose**:
  * Tests the kernel's ability to handle invalid stack pointers during system calls.
* **Behavior**:
  * Sets `%esp` to an invalid address.
  * Triggers a system call, expecting the kernel to terminate the process with `exit(-1)`.
* **Expected Outcome**:
  * The process should be terminated safely without crashing the kernel.

### 2 `sc-boundary-2.c`

* **Purpose**:
  * Tests handling of system call arguments that cross memory boundaries.
* **Behavior**:
  * Places syscall number or arguments across valid/invalid memory regions.
* **Expected Outcome**:
  * The kernel should detect the invalid access and terminate the process.

### Additional Considerations

#### Synchronization Between Parent and Child Processes

* **Use of Semaphores**:
  * Implement synchronization to ensure the parent process waits for the child to load successfully.
* **Load Success Flag**:
  * Use `load_success` in the PCB to communicate the load result.

#### Error Handling

* **Invalid Inputs**:
  * Validate all user inputs and pointers.