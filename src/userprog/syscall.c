// syscall.c

#include "userprog/syscall.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>

#define MAX_OPEN_FILES 128 // Maximum number of open files per process

static void syscall_handler(struct intr_frame* f);
static void check_pointer_valid(const void* p);
static void check_buffer_valid(const void* p, size_t size);
static bool is_valid_user_ptr(const void* ptr);
static char* copy_in_string(const char* us);

static void sys_exit(int status);
static int sys_practice(int i);
static pid_t sys_exec(const char* cmd_line);
static int sys_wait(pid_t pid);
static bool sys_create(const char* file, unsigned initial_size);
static bool sys_remove(const char* file);
static int sys_open(const char* file);
static int sys_filesize(int fd);
static int sys_read(int fd, void* buffer, unsigned size);
static int sys_write(int fd, const void* buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);
static struct file* get_file(int fd);

static struct lock filesys_lock; // Lock for synchronizing file system access

void syscall_init(void) {
  lock_init(&filesys_lock);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame* f) {
  check_pointer_valid(f->esp);
  uint32_t* args = (uint32_t*)f->esp;

  check_pointer_valid(args); // Validate the pointer to the syscall number
  uint32_t syscall_number = args[0];

  switch (syscall_number) {
  case SYS_HALT:
    shutdown_power_off();
    break;

  case SYS_EXIT:
    check_pointer_valid(args + 1);
    sys_exit((int)args[1]);
    break;

  case SYS_EXEC:
    check_pointer_valid(args + 1);
    f->eax = sys_exec((const char*)args[1]);
    break;

  case SYS_WAIT:
    check_pointer_valid(args + 1);
    f->eax = sys_wait((pid_t)args[1]);
    break;

  case SYS_CREATE:
    check_pointer_valid(args + 1);
    check_pointer_valid(args + 2);
    f->eax = sys_create((const char*)args[1], (unsigned)args[2]);
    break;

  case SYS_REMOVE:
    check_pointer_valid(args + 1);
    f->eax = sys_remove((const char*)args[1]);
    break;

  case SYS_OPEN:
    check_pointer_valid(args + 1);
    f->eax = sys_open((const char*)args[1]);
    break;

  case SYS_FILESIZE:
    check_pointer_valid(args + 1);
    f->eax = sys_filesize((int)args[1]);
    break;

  case SYS_READ:
    check_pointer_valid(args + 1);
    check_pointer_valid(args + 2);
    check_pointer_valid(args + 3);
    f->eax = sys_read((int)args[1], (void*)args[2], (unsigned)args[3]);
    break;

  case SYS_WRITE:
    check_pointer_valid(args + 1);
    check_pointer_valid(args + 2);
    check_pointer_valid(args + 3);
    f->eax = sys_write((int)args[1], (const void*)args[2], (unsigned)args[3]);
    break;

  case SYS_SEEK:
    check_pointer_valid(args + 1);
    check_pointer_valid(args + 2);
    sys_seek((int)args[1], (unsigned)args[2]);
    break;

  case SYS_TELL:
    check_pointer_valid(args + 1);
    f->eax = sys_tell((int)args[1]);
    break;

  case SYS_CLOSE:
    check_pointer_valid(args + 1);
    sys_close((int)args[1]);
    break;

  case SYS_PRACTICE:
    check_pointer_valid(args + 1);
    f->eax = sys_practice((int)args[1]);
    break;

  default:
    printf("Unknown syscall number: %d\n", syscall_number);
    sys_exit(-1);
  }
}

static bool is_valid_user_ptr(const void* ptr) {
  if (ptr == NULL || !is_user_vaddr(ptr))
    return false;

  struct thread* t = thread_current();
  if (t->pcb == NULL || t->pcb->pagedir == NULL)
    return false;

  return pagedir_get_page(t->pcb->pagedir, ptr) != NULL;
}

static void check_pointer_valid(const void* p) {
  if (!is_valid_user_ptr(p))
    sys_exit(-1);
}

static void check_buffer_valid(const void* p, size_t size) {
  const char* ptr = (const char*)p;
  for (size_t i = 0; i < size; i++) {
    if (!is_valid_user_ptr(ptr + i))
      sys_exit(-1);
  }
}

static char* copy_in_string(const char* us) {
  char* ks = malloc(128);
  if (ks == NULL)
    sys_exit(-1);

  size_t bufsize = 128;
  size_t i = 0;

  while (true) {
    check_pointer_valid(us + i);
    ks[i] = us[i];
    if (ks[i] == '\0')
      break;

    i++;
    if (i >= bufsize) {
      bufsize *= 2;
      char* new_ks = realloc(ks, bufsize);
      if (new_ks == NULL) {
        free(ks);
        sys_exit(-1);
      }
      ks = new_ks;
    }
  }
  return ks;
}

static void sys_exit(int status) {
  struct thread* cur = thread_current();
  cur->exit_status = status;
  thread_exit(); // Terminate the current thread
}

static int sys_practice(int i) { return i + 1; }

static pid_t sys_exec(const char* cmd_line) {
  check_pointer_valid(cmd_line);
  char* kcmd_line = copy_in_string(cmd_line);
  if (kcmd_line == NULL)
    return -1;

  pid_t pid = process_execute(kcmd_line);
  free(kcmd_line);
  return pid;
}

static int sys_wait(pid_t pid) { return process_wait(pid); }

static bool sys_create(const char* file, unsigned initial_size) {
  check_pointer_valid(file);
  char* kfile = copy_in_string(file);
  if (kfile == NULL)
    return false;

  lock_acquire(&filesys_lock);
  bool success = filesys_create(kfile, initial_size);
  lock_release(&filesys_lock);
  free(kfile);
  return success;
}

static bool sys_remove(const char* file) {
  check_pointer_valid(file);
  char* kfile = copy_in_string(file);
  if (kfile == NULL)
    return false;

  lock_acquire(&filesys_lock);
  bool success = filesys_remove(kfile);
  lock_release(&filesys_lock);
  free(kfile);
  return success;
}

static int sys_open(const char* file) {
  check_pointer_valid(file);
  char* kfile = copy_in_string(file);
  if (kfile == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  struct file* f = filesys_open(kfile);
  lock_release(&filesys_lock);
  free(kfile);

  if (f == NULL)
    return -1;

  struct thread* cur = thread_current();
  int fd;
  for (fd = 2; fd < MAX_OPEN_FILES; fd++) {
    if (cur->fd_table[fd] == NULL) {
      cur->fd_table[fd] = f;
      return fd;
    }
  }
  file_close(f);
  return -1;
}

static int sys_filesize(int fd) {
  struct file* f = get_file(fd);
  if (f == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  int length = file_length(f);
  lock_release(&filesys_lock);
  return length;
}

static int sys_read(int fd, void* buffer, unsigned size) {
  check_buffer_valid(buffer, size);

  if (fd == 0) { // STDIN
    for (unsigned i = 0; i < size; i++)
      ((uint8_t*)buffer)[i] = input_getc();
    return size;
  }

  struct file* f = get_file(fd);
  if (f == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  int bytes_read = file_read(f, buffer, size);
  lock_release(&filesys_lock);
  return bytes_read;
}

static int sys_write(int fd, const void* buffer, unsigned size) {
  check_buffer_valid(buffer, size);

  if (fd == 1) { // STDOUT
    putbuf(buffer, size);
    return size;
  }

  struct file* f = get_file(fd);
  if (f == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  int bytes_written = file_write(f, buffer, size);
  lock_release(&filesys_lock);
  return bytes_written;
}

static void sys_seek(int fd, unsigned position) {
  struct file* f = get_file(fd);
  if (f != NULL) {
    lock_acquire(&filesys_lock);
    file_seek(f, position);
    lock_release(&filesys_lock);
  }
}

static unsigned sys_tell(int fd) {
  struct file* f = get_file(fd);
  if (f == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  unsigned position = file_tell(f);
  lock_release(&filesys_lock);
  return position;
}

static void sys_close(int fd) {
  if (fd < 2 || fd >= MAX_OPEN_FILES)
    return;

  struct thread* cur = thread_current();
  struct file* f = cur->fd_table[fd];
  if (f != NULL) {
    lock_acquire(&filesys_lock);
    file_close(f);
    lock_release(&filesys_lock);
    cur->fd_table[fd] = NULL;
  }
}

static struct file* get_file(int fd) {
  struct thread* cur = thread_current();

  if (fd < 2 || fd >= cur->fd_table_size)
    return NULL;

  return cur->fd_table[fd];
}