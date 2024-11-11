/* In threads/fpu.c */

#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Floating Point Unit (FPU) */
void fpu_init(void) {
  uint16_t control_word;

  asm volatile("fninit");

  asm volatile("fstcw %0" : "=m"(control_word));
  control_word &= ~(0x300);
  control_word &= ~(0xC00);
  asm volatile("fldcw %0" ::"m"(control_word));
}