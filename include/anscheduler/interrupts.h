#ifndef __ANSCHEDULER_INTERRUPTS_H__
#define __ANSCHEDULER_INTERRUPTS_H__

#include "types.h"

#define ANSCHEDULER_PAGE_FAULT_PRESENT 1
#define ANSCHEDULER_PAGE_FAULT_WRITE 2
#define ANSCHEDULER_PAGE_FAULT_USER 4
#define ANSCHEDULER_PAGE_FAULT_INSTRUCTION 0x10

/**
 * Call this whenever a page fault or platform-equivalent interrupt occurs.
 *
 * This function should never return. Instead, it should switch directly
 * back into the current task if it wishes to proceed running. Otherwise, it
 * should call back to the run loop.
 * @critical
 */
void anscheduler_page_fault(void * ptr, uint64_t flags);

/**
 * Call this whenever an external IRQ comes in. When the IRQ arrives, save
 * the state of the current task. Then, if this function returns, restore the
 * state of the current task and continue running it. This is because this
 * function always has the possibility of not returning, but it could also
 * return if the interrupt thread is working on something.
 * @critical
 */
void anscheduler_irq(uint8_t irqNumber);

/**
 * @return The current thread which is registered to receive interrupts.
 * @critical
 */
thread_t * anscheduler_interrupt_thread();

/**
 * Set the current thread which will receive interrupts.
 * @critical
 */
void anscheduler_set_interrupt_thread(thread_t * thread);

/**
 * If the passed thread is the current interrupt thread, set the interrupt
 * thread to NULL. This should be called whenever a thread dies.
 * @critical
 */
void anscheduler_interrupt_thread_cmpnull(thread_t * thread);

/**
 * Get the system thread which is responsible for handling non-trivial
 * application page faults.
 * @critical
 */
thread_t * anscheduler_pager_thread();

/**
 * Set the system thread which is responsible for handling non-trivial
 * application page faults.
 * @critical
 */
void anscheduler_set_pager_thread(thread_t * thread);

/**
 * Get the next page fault, or NULL if no page faults are left to handle. Note
 * that, while the fault returned does include a referenced task, it will be
 * returned in a noncritical section. This is because it is presumed that the
 * system pager will NEVER die, and thus that the reference will not be
 * leaked.
 * @critical
 */
page_fault_t * anscheduler_page_fault_next();

#endif
