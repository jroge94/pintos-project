#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <stdint.h>

/* Maximum stack pages and threads (Project 2: Multithreading) */
#define MAX_STACK_PAGES (1 << 11)
#define MAX_THREADS 127

/* Process identifier type */
typedef tid_t pid_t;

/* Thread functions (Project 2: Multithreading) */
typedef void (*pthread_fun)(void*);
typedef void (*stub_fun)(pthread_fun, void*);

/* Process Control Block */
struct process {
    uint32_t* pagedir;          /* Page directory */
    int exit_status;            /* Exit status */
    char process_name[16];      /* Process name */
    struct thread* main_thread; /* Pointer to main thread */
};

/* Child process structure */
struct child_process {
    pid_t pid;
    int exit_status;
    bool wait_called;
    struct semaphore sema;
    struct list_elem elem;
};



/* Function prototypes */
void userprog_init(void);
pid_t process_execute(const char* file_name);
int process_wait(pid_t child_pid);
void process_exit(void);
void process_activate(void);

bool is_main_thread(struct thread* t, struct process* p);
pid_t get_pid(struct process* p);

tid_t pthread_execute(stub_fun sf, pthread_fun tf, void* arg);
tid_t pthread_join(tid_t tid);
void pthread_exit(void);
void pthread_exit_main(void);



/* Remove or comment out this line to prevent the conflicting declaration */
// bool load(const char* file_name, void (**eip)(void));

#endif /* userprog/process.h */
