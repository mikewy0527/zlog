/*
 * This file is part of the zlog Library.
 *
 * Copyright (C) 2018 by mikewy0527
 *
 * Licensed under the LGPL v2.1, see the file COPYING in base directory.
 */

#ifndef __ZLOG_ZC_ATOMIC_H
#define __ZLOG_ZC_ATOMIC_H

/* gcc version.
 * for example : v4.1.2 is 40102, v3.4.6 is 30406
 */
#define GCC_VERSION (__GNUC__ * 10000     \
				   + __GNUC_MINOR__ * 100 \
				   + __GNUC_PATCHLEVEL__)

#if (GCC_VERSION >= 40700)

#elif (GCC_VERSION >= 40102)
/* issues a full memory barrier. */
#define zc_barrier() __sync_synchronize()

#define ATOM_SET(ptr, value)        __sync_lock_test_and_set(ptr, value)

#define _INT_USLEEP (2)
#define ATOM_LOCK(ptr)                 \
    while(ATOM_SET(ptr, 1)) {          \
        usleep(_INT_USLEEP);           \
    }

#define ATOM_UNLOCK(ptr)            __sync_lock_release(ptr)

/* These built-in functions perform an atomic compare and swap. That is, if the
 * current value of *ptr is oldval, then write newval into *ptr.
 *
 * The “bool” version returns true if the comparison is successful and newval is
 * written. The “val” version returns the contents of *ptr before the operation.
 */
#define ATOM_CAS(ptr, oldval, newval)  \
	__sync_val_compare_and_swap(ptr, oldval, newval)

#define ATOM_CASB(ptr, oldval, newval) \
	__sync_bool_compare_and_swap(ptr, oldval, newval)

/* perform the operation suggested by the name, and return the new value.
 */
#define ATOM_ADD_F(ptr, value)      __sync_add_and_fetch(ptr, value)
#define ATOM_SUB_F(ptr, value)      __sync_sub_and_fetch(ptr, value)
#define ATOM_OR_F(ptr, value)       __sync_or_and_fetch(ptr, value)
#define ATOM_AND_F(ptr, value)      __sync_and_and_fetch(ptr, value)
#define ATOM_XOR_F(ptr, value)      __sync_xor_and_fetch(ptr, value)

/* perform the operation suggested by the name, and returns the value that had
 * previously been in memory.
 */
#define ATOM_F_ADD(ptr, value)      __sync_fetch_and_add(ptr, value)
#define ATOM_F_SUB(ptr, value)      __sync_fetch_and_sub(ptr, value)
#define ATOM_F_OR(ptr, value)       __sync_fetch_and_or(ptr, value)
#define ATOM_F_AND(ptr, value)      __sync_fetch_and_and(ptr, value)
#define ATOM_F_XOR(ptr, value)      __sync_fetch_and_xor(ptr, value)

#else
# error "can not supported atomic operation by gcc(v4.0.0+) buildin function."
#endif  /* if (GCC_VERSION >= 40100) */

#endif
