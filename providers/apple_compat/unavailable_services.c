#include <stdint.h>
#include <stdlib.h>

extern const void* CFArrayCreate(const void*, const void**, int64_t, const void*);

void* IOHIDEventSystemClientCreate(const void* allocator)
{
	(void)allocator;
	return calloc(1, 1);
}

void IOHIDEventSystemClientSetMatching(void* client, const void* matching)
{
	(void)client;
	(void)matching;
}

const void* IOHIDEventSystemClientCopyServices(void* client)
{
	(void)client;
	return CFArrayCreate(NULL, NULL, 0, NULL);
}

const void* IOHIDServiceClientCopyEvent(void* service, int64_t type,
	int32_t options, int64_t timeout)
{
	(void)service;
	(void)type;
	(void)options;
	(void)timeout;
	return NULL;
}

const void* IOHIDServiceClientCopyProperty(void* service, const void* key)
{
	(void)service;
	(void)key;
	return NULL;
}

double IOHIDEventGetFloatValue(const void* event, int32_t field)
{
	(void)event;
	(void)field;
	return 0.0;
}

int32_t LSOpenCFURLRef(const void* url, const void** launched_url)
{
	(void)url;
	if (launched_url)
		*launched_url = NULL;
	return -4;
}

void* CFRunLoopGetCurrent(void)
{
	static _Thread_local int run_loop;
	return &run_loop;
}

uint8_t CFRunLoopIsWaiting(void* run_loop)
{
	(void)run_loop;
	return 0;
}

void CFRunLoopRun(void)
{
}

void CFRunLoopStop(void* run_loop)
{
	(void)run_loop;
}

void CFRunLoopWakeUp(void* run_loop)
{
	(void)run_loop;
}
