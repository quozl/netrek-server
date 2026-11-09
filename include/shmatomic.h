#ifndef SHMATOMIC_H
#define SHMATOMIC_H

/* Ordered access to the shared memory segment, which the daemon, the
   ntserv processes and the robots use at the same time when there is
   more than one CPU.  See TEST_RESULTS_SHM_STRESS.md.

   A writer fills in an entry, then publishes it with store_release(),
   such as by moving a queue index to it.  A reader that sees the
   published value with load_acquire() also sees the entry complete.
   Neither costs an instruction on x86, but each stops the compiler
   from moving other accesses across it. */

#define load_acquire(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define store_release(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)

/* A count changed by more than one process, such as p_ntorp, must be
   changed in one indivisible step, or one change may undo another. */

#define atomic_inc(p) __atomic_add_fetch((p), 1, __ATOMIC_RELAXED)
#define atomic_dec(p) __atomic_sub_fetch((p), 1, __ATOMIC_RELAXED)

/* The same for flags changed by more than one process, such as
   p_flags: set, clear or toggle bits in one indivisible step. */

#define atomic_or(p, v) __atomic_or_fetch((p), (v), __ATOMIC_RELAXED)
#define atomic_and(p, v) __atomic_and_fetch((p), (v), __ATOMIC_RELAXED)
#define atomic_xor(p, v) __atomic_xor_fetch((p), (v), __ATOMIC_RELAXED)

/* Change *p from old to new in one indivisible step, as a release
   store, if *p is old, and return true; otherwise leave it and return
   false.  Use it where a test and a change of state must not be
   separated, such as detonating a torpedo that is still moving. */

static inline int compare_and_swap(int *p, int old, int new)
{
  return __atomic_compare_exchange_n(p, &old, new, 0,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

#endif /* SHMATOMIC_H */
