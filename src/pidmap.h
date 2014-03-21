#ifndef __ANSCHEDULER_PIDMAP_H__
#define __ANSCHEDULER_PIDMAP_H__

#include <anscheduler/types.h>

/**
 * @critical O(1)
 */
uint64_t anscheduler_pidmap_alloc_pid();

/**
 * @critical O(1)
 */
void anscheduler_pidmap_free_pid(uint64_t pid);

/**
 * @critical O(1)
 */
void anschedulrer_pidmap_set(task_t * task);

/**
 * @critical O(1)
 */
void anschedulrer_pidmap_unset(task_t * task);

/**
 * @critical Best is O(1), worst is O(n)
 */
task_t anschedulrer_pidmap_get(uint64_t pid);

#endif
