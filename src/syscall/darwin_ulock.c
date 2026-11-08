#include "darwin_ulock.h"
#include "ports/user_lock.h"

#include <errno.h>

#define DARWIN_UL_OPCODE_MASK 0x000000ffu
#define DARWIN_ULF_WAKE_ALL 0x00000100u
#define DARWIN_ULF_WAKE_THREAD 0x00000200u
#define DARWIN_ULF_WAKE_ALLOW_NON_OWNER 0x00000400u
#define DARWIN_ULF_DEADLINE 0x02000000u
#define DARWIN_ULF_WAIT_MASK 0x03070000u
#define DARWIN_ULF_WAKE_MASK 0x01000700u

static unsigned user_lock_width(uint32_t opcode)
{
	switch (opcode) {
	case 1: case 2: case 3: return 4;
	case 5: case 6: return 8;
	default: return 0;
	}
}

int machgate_darwin_ulock_wait(uint32_t operation, void* address, uint64_t value,
                              uint64_t timeout_ns)
{
	uint32_t opcode = operation & DARWIN_UL_OPCODE_MASK;
	unsigned width = user_lock_width(opcode);
	if (!width || !address || ((uintptr_t)address & (width - 1)) ||
	    (operation & ~(DARWIN_UL_OPCODE_MASK | DARWIN_ULF_WAIT_MASK)))
		return -EINVAL;
	return machgate_wait_on_word(address, value, width, opcode == 3 || opcode == 6,
	                             timeout_ns, (operation & DARWIN_ULF_DEADLINE) != 0);
}

int machgate_darwin_ulock_wake(uint32_t operation, void* address, uint64_t wake_value)
{
	(void)wake_value;
	uint32_t opcode = operation & DARWIN_UL_OPCODE_MASK;
	if (!user_lock_width(opcode) || !address ||
	    (operation & ~(DARWIN_UL_OPCODE_MASK | DARWIN_ULF_WAKE_MASK)))
		return -EINVAL;
	if ((operation & DARWIN_ULF_WAKE_ALLOW_NON_OWNER) && opcode != 2)
		return -EINVAL;
	if (operation & DARWIN_ULF_WAKE_THREAD) {
		if ((operation & DARWIN_ULF_WAKE_ALL) || opcode == 2)
			return -EINVAL;
		return -ENOTSUP;
	}
	return machgate_wake_word(address, opcode == 3 || opcode == 6,
	                          (operation & DARWIN_ULF_WAKE_ALL) != 0);
}
