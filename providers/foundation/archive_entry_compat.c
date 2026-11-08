#include "call_frame.h"

#include <stddef.h>
#include <string.h>

struct archive_forwarder_arguments {
	void* implementation;
	uint64_t stack_count;
	void* receiver;
	void* selector;
	void* indirect_result;
	uintptr_t registers[6];
	const uintptr_t* stack;
};

struct archive_message_result {
	void* pointer;
	double floating;
};

_Static_assert(offsetof(struct archive_forwarder_arguments, stack) == 88,
	"Archive forwarder stack offset");

void* shim_objc_forwarder(struct archive_forwarder_arguments* arguments);

void* shim_objc_msgSend_call_guest_imp(void* implementation, void* receiver,
	void* selector, void* indirect_result, const uintptr_t* registers,
	const uintptr_t* stack)
{
	if (!implementation || !registers)
		return NULL;
	struct archive_forwarder_arguments arguments = {
		.implementation = implementation, .stack_count = stack ? 16 : 0,
		.receiver = receiver, .selector = selector,
		.indirect_result = indirect_result, .stack = stack,
	};
	memcpy(arguments.registers, registers, sizeof(arguments.registers));
	return shim_objc_forwarder(&arguments);
}

struct archive_message_result shim_objc_msgSend_struct_impl(void* receiver,
	void* selector, uintptr_t argument2, uintptr_t argument3, uintptr_t argument4,
	uintptr_t argument5, uintptr_t argument6, uintptr_t argument7,
	void* indirect_result, const uintptr_t* stack)
{
	struct foundation_call call = {
		.arguments = {(uintptr_t)receiver, (uintptr_t)selector, argument2,
			argument3, argument4, argument5, argument6, argument7,
			(uintptr_t)indirect_result},
		.stack = stack,
	};
	foundation_dispatch(&call);
	if (call.fallback)
		call.result = (uintptr_t)shim_objc_msgSend_call_guest_imp(call.fallback,
			receiver, selector, indirect_result, &call.arguments[2], stack);
	return (struct archive_message_result){(void*)call.result, call.floating};
}

void* shim_objc_msgSend_impl(void* receiver, void* selector,
	uintptr_t argument2, uintptr_t argument3, uintptr_t argument4,
	uintptr_t argument5, uintptr_t argument6, uintptr_t argument7,
	void* indirect_result, const uintptr_t* stack)
{
	return shim_objc_msgSend_struct_impl(receiver, selector, argument2,
		argument3, argument4, argument5, argument6, argument7,
		indirect_result, stack).pointer;
}
