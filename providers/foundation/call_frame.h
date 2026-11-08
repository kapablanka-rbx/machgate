#ifndef MACHGATE_FOUNDATION_CALL_FRAME_H
#define MACHGATE_FOUNDATION_CALL_FRAME_H

#include <stdint.h>

struct foundation_call {
	uintptr_t arguments[9];
	const uintptr_t* stack;
	__uint128_t vectors[8];
	uintptr_t result;
	double floating;
	void* fallback;
};

void foundation_dispatch(struct foundation_call* call);

#endif
