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

- Briefly describe how you implemented argument parsing.  How do you arrange for the
  elements of `argv[]` to be in the right order? How do you avoid overflowing the
  stack page?

### Argument Passing - Rationale

- Why does PintOS implement `strtok_r()` but not `strtok()`?
- In PintOS, the kernel separates commands into a executable name and arguments.  In
  Unix-like systems, the shell does this separation. Identify at least two 
  advantages of the Unix approach.

## System Calls

Provide the following four sections for each of the syscall tasks (process control,
file operations, and floating point operations).

### System Calls - Data Structures

- Copy here the declaration of each new or changed struct or struct member, global or
  static variable, typedef, or enumeration.  Identify the purpose of each in 25 words
  or less.
- Describe how file descriptors are associated with open files. Are file descriptors
  unique within the entire OS or just within a single process?

### System Calls - Algorithms

- Describe your code for reading and writing user data from the kernel.
- Suppose a system call causes a full page (4,096 bytes) of data to be copied from
  user space into the kernel.  What is the least and the greatest possible number of
  inspections of the page table (e.g. calls to `pagedir_get_page()`) that might
  result?  What about for a system call that only copies 2 bytes of data?  Is there
  room for improvement in these numbers, and how much?
- Briefly describe your implementation of the "wait" system call and how it
  interacts with process termination.
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

### System Calls - Synchronization

- Study the interface of the semaphore functions and describe the sequence of calls
  required for a thread to be able to wait for an event that happens in another
  thread.
- The `exec` system call returns `-1` if loading the new executable fails, so it
  cannot return before the new executable has completed loading.  How does your code
  ensure this?  How is the load success/failure status passed back to the thread that
  calls "exec"?
- Consider parent process P with child process C.  How do you ensure proper
  synchronization and avoid race conditions when P calls `wait(C)` before `C` exits? 
  After `C` exits?  How do you ensure that all resources are freed in each case?  How
  about when `P` terminates without waiting, before `C` exits?  After `C` exits?  Are
  there any special cases?

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
