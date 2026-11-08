#include <stdint.h>

enum { MACH_NOT_SUPPORTED = 46 };

int thread_info(uint32_t thread, int flavor, void* output, uint32_t* count)
{
	(void)thread;
	(void)flavor;
	(void)output;
	(void)count;
	return MACH_NOT_SUPPORTED;
}

int thread_get_state(uint32_t thread, int flavor, void* output, uint32_t* count)
{
	(void)thread;
	(void)flavor;
	(void)output;
	(void)count;
	return MACH_NOT_SUPPORTED;
}

int thread_suspend(uint32_t thread)
{
	(void)thread;
	return MACH_NOT_SUPPORTED;
}

int thread_resume(uint32_t thread)
{
	(void)thread;
	return MACH_NOT_SUPPORTED;
}

int thread_set_exception_ports(uint32_t thread, uint32_t exception_mask,
	                       uint32_t port, int behavior, int flavor)
{
	(void)thread;
	(void)exception_mask;
	(void)port;
	(void)behavior;
	(void)flavor;
	return MACH_NOT_SUPPORTED;
}

int mach_port_allocate(uint32_t task, uint32_t right, uint32_t* name)
{
	(void)task;
	(void)right;
	(void)name;
	return MACH_NOT_SUPPORTED;
}

int mach_port_construct(uint32_t task, void* options, uint64_t context,
	                uint32_t* name)
{
	(void)task;
	(void)options;
	(void)context;
	(void)name;
	return MACH_NOT_SUPPORTED;
}

int mach_port_insert_right(uint32_t task, uint32_t name, uint32_t right,
	                   uint32_t disposition)
{
	(void)task;
	(void)name;
	(void)right;
	(void)disposition;
	return MACH_NOT_SUPPORTED;
}

int mach_port_set_attributes(uint32_t task, uint32_t name, int flavor,
	                     void* information, uint32_t count)
{
	(void)task;
	(void)name;
	(void)flavor;
	(void)information;
	(void)count;
	return MACH_NOT_SUPPORTED;
}

int mach_msg_server_once(void* demultiplexer, uint32_t maximum_size,
	                 uint32_t receive_name, uint32_t options)
{
	(void)demultiplexer;
	(void)maximum_size;
	(void)receive_name;
	(void)options;
	return MACH_NOT_SUPPORTED;
}
