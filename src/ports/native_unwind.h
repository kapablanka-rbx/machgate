#ifndef MACHGATE_PORTS_NATIVE_UNWIND_H
#define MACHGATE_PORTS_NATIVE_UNWIND_H

void machgate_guest_register_frame(const void* frame);
void machgate_guest_deregister_frame(const void* frame);

#endif
