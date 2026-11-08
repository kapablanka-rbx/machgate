#ifndef MACHGATE_PORTS_NATIVE_BLOCKS_H
#define MACHGATE_PORTS_NATIVE_BLOCKS_H

void machgate_register_block_object_lifetime_v1(void* (*retain)(void*), void (*release)(void*));
int machgate_is_block_v1(const void* object);
void* _Block_copy(const void* block);
void _Block_release(const void* block);

#endif
