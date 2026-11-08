#include "darwin_errno.h"
#include "syscall/darwin_ulock.h"

#include <errno.h>

static int user_lock_result(int result, uint32_t operation)
{
	if (result >= 0)
		return result;
	int darwin_errno = machgate_darwin_errno_from_linux(-result);
	if (operation & MACHGATE_ULF_NO_ERRNO)
		return -darwin_errno;
	errno = darwin_errno;
	return -1;
}

int __ulock_wait2(uint32_t operation, void* address, uint64_t value,
                  uint64_t timeout, uint64_t value2)
{
	(void)value2;
	return user_lock_result(machgate_darwin_ulock_wait(operation, address, value,
	                                                 timeout), operation);
}

int __ulock_wait(uint32_t operation, void* address, uint64_t value, uint32_t timeout)
{
	return user_lock_result(machgate_darwin_ulock_wait(operation, address, value,
	                                                 (uint64_t)timeout * 1000), operation);
}

int __ulock_wake(uint32_t operation, void* address, uint64_t wake_value)
{
	return user_lock_result(machgate_darwin_ulock_wake(operation, address, wake_value),
	                        operation);
}
