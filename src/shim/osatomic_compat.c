#include <stdint.h>

uint8_t OSCompareAndSwap64(uint64_t old_value, uint64_t new_value,
                           volatile uint64_t* address)
{
	return __atomic_compare_exchange_n(address, &old_value, new_value, 0,
	                                   __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

int64_t OSAddAtomic64(int64_t amount, volatile int64_t* address)
{
	return __atomic_fetch_add(address, amount, __ATOMIC_RELAXED);
}

int64_t OSIncrementAtomic64(volatile int64_t* address)
{
	return OSAddAtomic64(1, address);
}

int64_t OSDecrementAtomic64(volatile int64_t* address)
{
	return OSAddAtomic64(-1, address);
}
