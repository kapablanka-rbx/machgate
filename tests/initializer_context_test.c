#include "shim/initializer_context.h"

int main(void)
{
	machgate_shim_note_init_context("pointer", 1, 3, 0x1234);
	machgate_trace_initializer_context();
	machgate_shim_clear_init_context();
	machgate_trace_initializer_context();
	return 0;
}
