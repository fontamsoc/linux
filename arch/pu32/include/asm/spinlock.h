// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_SPINLOCK_H
#define __ASM_PU32_SPINLOCK_H

#include <asm/spinlock_types.h>
#include <asm/cmpxchg.h>
#include <asm/processor.h>

#define arch_spin_is_locked(x)	((x)->lock[1] != (x)->lock[2])

static inline int arch_spin_trylock (arch_spinlock_t *m) {
	int ret = 0;
	if (xchg(&m->lock[0], 1))
		goto done;
	if (arch_spin_is_locked((arch_spinlock_t *)m))
		goto done_unlock;
	++m->lock[1];
	ret = 1;
	done_unlock:
	xchg(&m->lock[0], 0); // memory fenced version of: m->lock[0] = 0;
	done:
	return ret;
}

static inline void arch_spin_lock (arch_spinlock_t *m) {
	while (xchg(&m->lock[0], 1))
		cpu_relax();
	while ((m->lock[1] - m->lock[2]) == -1)
		cpu_relax();
	unsigned long i = m->lock[1]++;
	xchg(&m->lock[0], 0); // memory fenced version of: m->lock[0] = 0;
	while (m->lock[2] != i)
		cpu_relax();
}

static inline void arch_spin_unlock (arch_spinlock_t *m) {
	xchg(&m->lock[2], (m->lock[2]+1)); // memory fenced version of: ++m->lock[2];
}

// Read-write spinlocks, allowing multiple readers but only one writer.
// When readers hold the lock, prospective writers spinlock in the queue.
// When a writer hold the lock, prospective readers & writers spinlock in queue.

static inline int arch_read_trylock (arch_rwlock_t *m) {
	int ret = 0;
	if (xchg(&m->lock[0], 1))
		goto done;
	if (arch_spin_is_locked((arch_spinlock_t *)m))
		goto done_unlock;
	++m->lock[3];
	ret = 1;
	done_unlock:
	xchg(&m->lock[0], 0); // memory fenced version of: m->lock[0] = 0;
	done:
	return ret;
}

static inline void arch_read_lock (arch_rwlock_t *m) {
	while (xchg(&m->lock[0], 1))
		cpu_relax();
	if (arch_spin_is_locked((arch_spinlock_t *)m)) {
		while ((m->lock[1] - m->lock[2]) == -1)
			cpu_relax();
		unsigned long i = m->lock[1]++;
		xchg(&m->lock[0], 0); // memory fenced version of: m->lock[0] = 0;
		while (m->lock[2] != i)
			cpu_relax();
		++m->lock[3];
		xchg(&m->lock[2], (m->lock[2]+1)); // memory fenced version of: ++m->lock[2];
		return;
	}
	++m->lock[3];
	xchg(&m->lock[0], 0); // memory fenced version of: m->lock[0] = 0;
}

static inline void arch_read_unlock (arch_rwlock_t *m) {
	while (xchg(&m->lock[0], 1))
		cpu_relax();
	--m->lock[3];
	xchg(&m->lock[0], 0); // memory fenced version of: m->lock[0] = 0;
}

static inline int arch_write_trylock (arch_rwlock_t *m) {
	if (!arch_spin_trylock((arch_spinlock_t *)m))
		return 0;
	if (m->lock[3]) {
		arch_spin_unlock((arch_spinlock_t *)m);
		return 0;
	}
	return 1;
}

static inline void arch_write_lock (arch_rwlock_t *m) {
	arch_spin_lock((arch_spinlock_t *)m);
	while (m->lock[3])
		cpu_relax();
}

static inline void arch_write_unlock (arch_rwlock_t *m) {
	arch_spin_unlock((arch_spinlock_t *)m);
}

#endif /* __ASM_PU32_SPINLOCK_H */
