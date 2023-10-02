#include <sys/lock.h>
#include <stdlib.h>
#include "atommutex.h"

#if !defined(_RETARGETABLE_LOCKING) || (_RETARGETABLE_LOCKING == 0)
#error "Newlib not compiled with retargetable lock functions"
#endif

/* atomMutexCreate() simply clears all fields in the mutex struct,
 * so a statically allocated ATOM_MUTEX should already be zeroed / initialized
 */

struct __lock {
  ATOM_MUTEX mutex;
};

struct __lock __lock___sinit_recursive_mutex;
struct __lock __lock___sfp_recursive_mutex;
struct __lock __lock___atexit_recursive_mutex;
struct __lock __lock___at_quick_exit_mutex;
struct __lock __lock___malloc_recursive_mutex;
struct __lock __lock___env_recursive_mutex;
struct __lock __lock___tz_mutex;
struct __lock __lock___dd_hash_mutex;
struct __lock __lock___arc4random_mutex;

void __retarget_lock_init(_LOCK_T* l) {
	*l = (_LOCK_T)malloc(sizeof(**l));
	if (*l != NULL)
		atomMutexCreate(&((*l)->mutex));
}

void __retarget_lock_init_recursive(_LOCK_T* l) {
	*l = (_LOCK_T)malloc(sizeof(**l));
	if (*l != NULL)
		atomMutexCreate(&((*l)->mutex));
}

void __retarget_lock_close(_LOCK_T l) {
	if (l != NULL) {
		atomMutexDelete(&l->mutex);
		free(l);
	}
}

void __retarget_lock_close_recursive(_LOCK_T l) {
	if (l != NULL) {
		atomMutexDelete(&l->mutex);
		free(l);
	}
}

void __retarget_lock_acquire(_LOCK_T l) {
	atomMutexGet(&l->mutex, 0);
}

void __retarget_lock_acquire_recursive(_LOCK_T l) {
	atomMutexGet(&l->mutex, 0);
}

int __retarget_lock_try_acquire(_LOCK_T l) {
	if (atomMutexGet(&l->mutex, -1) == ATOM_OK)
		return 1;
	return 0;
}

int __retarget_lock_try_acquire_recursive(_LOCK_T l) {
	if (atomMutexGet(&l->mutex, -1) == ATOM_OK)
		return 1;
	return 0;
}

void __retarget_lock_release(_LOCK_T l) {
	atomMutexPut(&l->mutex);
}

void __retarget_lock_release_recursive(_LOCK_T l) {
	atomMutexPut(&l->mutex);
}
