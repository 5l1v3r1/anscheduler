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
 * Call this whenever an external IRQ comes in.
 */
void anscheduler_irq(uint8_t irqNumber);
