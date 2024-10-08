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

  # Checkpoint Two - File Operation Syscalls
#### tshime1
### Data Structures and Functions
```c
	/*Functions to check validity of pointers*/

	static void check_pointer_valid(const void* p){
	//Given in lecture
 	//I think we all listed this function but called it something a little different. It is to be used before every function call in this part though
	//Check if pointer is valid, call sys_exit(-1) if not
}
	static void check_buffer_valid(const void* p, size_t size){
	//Given in lecture
	//Iterates with check_pointer_valid(p) function to check if buffer is valid
}

/*Create an struct syscall_args and function to handle each syscall*/

	//Structures which hold the arguments to the system call functions

	struct create_args {
		int id;                //Syscall id (
		const char *file;      //File name to be created
		unsigned initial_size; //Initial byte size of the file
};
	struct remove_args{
		int id;			//Syscall id
		const char *file;	//Name of file to be deleted
};
	//Create struct args for each in same fashion as above, cooresponding to their related function below, 
	//but will omit that to keep this a bit more condensed

	struct open_args{...};
	struct filesize_args{...};
	struct read_args{...};
	struct write_args{...};
	struct seek_args{...};
	struct tell_args{...};
	struct close_args{...};
	
	/*Handler functions for each of the file operation syscalls*/
	//These are all pretty self explanatory, but explained a bit more in algorithm section
	bool create (const char *file, unsigned initial_size){}
	bool remove (const char *file){}
	int open (const char *file){}
	int filesize (int fd){}
	int read (int fd, void *buffer, unsigned size){}
	int write (int fd, const void *buffer, unsigned size){}
	void seek (int fd, unsigned position){}
	int tell(int fd){}
	void close (int fd){}

 //Finally a struct to keep track of our file descriptors
 struct file_descriptor {
    int fd;                
    struct file *file;     
    struct list_elem elem;
 //will also need a function to link a fd to a file using this struct
 //and then a larger fd_list, this was done before, so not a huge concern with implemntation
 
};
```

### Algorithms
#### Plans for writing code
* To start off, restructure the syscall_handler function into a switch statement.
	*  Easier to maintain and understand than a cascade of if statements. No functional difference, just for ease of use.
  ```c
	 switch(args[0]){
	      case SYS_CREATE: ...; //cast arguments at stack pointer to the cooresponding syscall struct already created, then call function
		case SYS_REMOVE: ...; //example cast - struct syscall_args *args = (struct remove_args *) f->esp;
		case SYS_OPEN: ...;
		case SYS_FILESIZE: ...; 
		case SYS_READ: ...;
		case SYS_WRITE: ...;
		case SYS_SEEK: ...;
		case SYS_TELL: ...;
		case SYS_CLOSE:  ...;
	}
  ```
* Create a file descriptor list starting after the standard fd values (0 and 1 are already used for STDIN/OUT)
	*  Each process should have its own list. So while fd values might not be unique between processes, the files they point to are different
	*  Use an entry method to this list just like we did in the previous assignment.
	*  The addition of a fd list requires more attention to be spent on avoiding memory leaks when adding and removing.
* Use a global lock when handling file operations (temporarily, will improve later)
 *  For now, all of the file operation functions will be treated as critical (not forgetting to
	   unlock before returning).
	
#### Function explanations
* using file.h and filesys.h functions, the functions become pretty simple, mostly forwarding to other functions (outside of checking for edge cases and resolving them, and locking if electing to do it inside the functions).
* In the case that we do it that way, just add an acquire lock statement at the start of all of these functions (or around the actual code where it accesses shared data to reduce time spent with the lock) 
	* Pointers and buffers will be validated using aforementioned functions and all edge cases where inputs are NULL will be accounted for likely by exiting the process with some error message. *Unless it makes sense to perform the operation on STDOUT or STDIN
	* Add pointer validation when writing actual code, left out for now

* create
     - forward args->file and args->initial_size to the filesys_create() function
     - returns true if successful in creating file
* remove
     - forward args to the filesys_remove function
     - returns true on success
* open
     - forward args to filesys_open function
     - if null return -1
     - on success,remember to free memory and remove from list when closing file
     - returns file descriptor value
     - don't forget to add to the fd_list, link the file and the fd value.

 ***When fd = 0 or fd = 1, use I/O functions in src/lib/stdio.h instead of the normal file operation functions being used***
    - not completely sure how they work, but they seem to be analogs to 
* filesize
     - get the file at fd,check if null, 0, or 1
     - use file_length function on file
     - return size
       
* read
     - check if standard input, read from stdin if 1
     - get file at fd, if null return -1
     - use file_read function.
* write (an implementation of what Joel said above)
     - check if stdout, write to out then return bytes written - use putbuf(buffer,size).
     - open file, if null return -1.
     - then return int bytes from the file_write() function.
     - pay attention to NOTE here - if buffer is significantly big break it into smaller parts and repeat with multiple parts?
* seek
     - access file at fd, if null do nothing.
     - check that you aren't seeking past the end of the file.
     - leave a comment here as it seems we will change this later when we can go beyond the file limits.
     - use file_seek() to set the position in the file.
* tell
     - access file, return -1 if null.
     - return int from file_tell function.
* close
     - access file, do nothing if null.
		   - use file_close, then remove fd from the fd list.
		   - free any resources associated.
	* exit
     - exit is already implemented, but whenever these new changes are implemented, will have to add to the exit function
     - on process exit, make sure to clean up => close all file descriptors
	
#### Synchronization
* If no locks are implemented, it would be possible for the program to run multiple file operations at one time. This is obviously not desirable.
* Described in the algorithms part, the simple solution for now will be to just use a global lock around the file operation functions.
	* This solution isn't the best and is a bit cumbersome. This is a lot of time locked and a lot of overhead having to wait to acquire the locks.
	* Bottleneck around these system calls as every single one of these function calls has to wait for a lock to become available and then it locks preventing others from acting.
	* Despite the drawbacks, this implementation is the easiest synchronization option and the simplest way to ensure there are no race conditions/deadlocks.
 * This may significantly limit concurrency and there will likely be lots of contention, but project specifications say to do this for now.
	

#### Rationale
* There isn't a ton of design room here; simply trying to implement desired functionality listed in project1. Most of it is also just sending stuff to other functions to handle the work.
* One choice made here, segmenting the code, would make it much easier to maintain and bug fix in the future.
* Doing this bit of extra work to split everything into all of these little parts will also make the addition of new features simpler (really just all around, as it is easier initially too).
	
* The global lock system is the easiest synchronization solution to implement, but not as efficient. The simpler locking mechanism makes debugging much easier and lessens room for error.
* Also, using a simpler lock initially allows us to make sure that our code works without having to worry too much about 
	   figuring out synchronization for now. It also shouldn't be too hard to upgrade in the future.

#### Jack Thomas
#### Design Document
# Process controls

## Data structures and functions

/* Structure to represent child processes. */
struct child_process {
    tid_t tid;                   // Thread ID of the child.
    int exit_status;             // Exit status of the child.
    bool wait_istrue;            // Whether wait() has been called on this child.
    struct semaphore sema_wait;  // Semaphore for parent to wait on child.
    struct list_elem elem;       // List element for child list.
};

This structure holds information about child processes, and helps facilitate synchronization and the status of a child process.

/* Modifying struct thread in thread.h */
struct list child_list;          // List of child processes.
struct child_process *cp;        // Pointer to own child_process struct.
struct semaphore sema_exec;      // Semaphore for synchronization during exec
struct semaphore sema_exit;      // Semaphore for synchronization during exit
bool load_success;               // Checks if process has loaded succesfully

Global variables



### Functions

bool validate_user_pointer(const void *ptr);
// Checks if user pointer is valid
bool copy_in(void *dst, const void *usrc, size_t size)
// Safely copying data from user to kernel space
bool copy_out(void *udst, const void *src, size_t size)
// Safely copying data from kernel space to user space

### Algorithms 

Accessing user memory Safely

1. We first would use validate_user_pointer to ensure the pointer is valid.
2. We use copy_in() and copy_out() to transfer data between user and kernel

#### Implementaiton of system calls

Inside of syscall_handler function in syscalls.c
  
#### Implementation of practice syscall

1. Retrieve system call argument from user stack
2. Validate user pointer with validate_user_pointer function
3. Safely copy the integer argument into kernel space from user space
4. Call the syscall implementation and set return value
    - f->eax = i + 1

#### Implementation of halt syscall
1. if args[0] = SYS_HALT
2. call function shutdown_power_off()

#### Implementation of wait sys call
Parent process
1. Retrieve and validate PID argument from user stack.
1. Traverse child_list to find the child_process with given PID.
2. Check if child is already waited on, if it is return -1 to prevent multiple waits.
We also need to check here if the PID corresponds to a direct child or not, if it does not wait() should return -1.
We also need to make sure that if the child did not call exit() and was instead terminated by the kernel, wait() should return -1.
3. If not, set wait_istrue to true and call sema_down(&child->sema_wait) to block until child signals termination.
4. After unblocking, get exit status of child and remove it from child_list. Free its memory.
5. Return exit status to user program
Child process on exit
1. In process_exit(), set cp->exit_status to the process's exit status.
2. Call sema_up(&cp->sema_wait) to unblock any waiting parent 
3. Cleanup resources

#### Implementation of exec System Call

Parent process
1. Initialize synchronization
  - Initialize sema_init and thread_current->load_success = false
2. Execute process_execute to create the child process
3. Call sema_down to wait for child to signal
4. Check load success with load_success is true/false
5. If load_success is true, return the child's PID, else return -1

In Child process
1. Attempt to load exec
2. Set load status to true if succesfull and false if unsuccesfull.
3. Signal parent to unblock if successful with sema_up

#### Implementation of exit system call

1. Get exit status argument from user stack
2. Validate pointer and copy the exit status with copy_in()
3. Set the current threads exit status to the retrieved exit status and print exit message
4. Signal possible waiting parent process using sema_up
5. Close files and call thread_exit to terminate the process.

### Synchronization

1. struct child_process
    - Accessed by both parent and child processes
        The parent process initializes a child_process struct for each child and adds it to child_list
        The child process has a pointer to its own child_process struct which it updates when exiting
    - Synchronization
        We use semaphores like sema_wait and sema_exit within child_process struct to make sure the parent and child are not
        modifying the same data concurrently as well as avoiding race conditions.
        The parent process modifies the child_process struct when creating and removing it from child_list after wait().
        The child process updates its exit_status and signals the parents when it exits.
2. child_list in struct thread
    -Accesed by the parent process
        The parent will add new child_process structs to the child_list when its creating child processes.
        It will also remove child_process structs from child_list after calling wait() or when exiting
    - Synchronization
        Only the parent thead modifies its own child_list, so no other thread can access it. This also avoids race conditions.
3. semaphores
    - Accesed by both parent and child processes
        sema_exec - Parent initializes and waits after calling process_execute(), the child signals it after attempting to load executable.
        sema_wait- Parent waits on it durin wait() until the child exits, and the child signals upon exiting.
        sema_exit - Child waits on it after signaling the parent when exiting, and parent signals it after it retrieves the child's exit status.
    - Synchronization
    - Using semaphores enforces the parent and child to have a strict order of operations it must go through to ensure synchronization, which if done correctly
    will ensure that the threads wait for the necessary signals before proceeding.
4. load_success 
    - Accesed by both parent and child processes
        Child process sets it to true or false depending on if loading the executable worked. The parent process reads load_success after being signaled by child.
    - Synchronization
        The parent will wait on sema_exec and only until after the child signals will it read load_success.
5. exit_status 
    - Accessed by both child and parent processes
        The child process will write the exit status upon exiting, and the parent process will read the status after its signaled by the child.
    - Synchronization
        The parent will wait on sema_wait and only read the exit status until after the child has signaled to the parent process.
        
### Rationale

This design aims to provide a clear and efficient implenetation of the required functionalities for processs control. We use simple data structures like struct child_process 
to make it easy to manage child-specific information and synchronization. We also utilize clear synchronization mechanisms with the use of sema_wait, sema_exec, and sema_exit to synchronize between parent
and child processes.

We tried to add reusability into our code, for functions like validate_user_pointer and copy_in and copy_out. These are functions designed to reuse for multiple system calls we 
will implement in this part of the project. This also would allow for us to easily add more system calls if we needed, as we can call these functions in them to accomplish tasks
we would have had to complete. We also do not add much overhead for the time complexity of the programs as semaphores are only used so often and they do not seriously
impact performance.
