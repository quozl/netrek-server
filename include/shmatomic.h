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

#endif /* SHMATOMIC_H */
