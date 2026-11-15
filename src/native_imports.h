#ifndef MACHGATE_NATIVE_IMPORTS_H
#define MACHGATE_NATIVE_IMPORTS_H

#include "domain/guest_image.h"

#include <stddef.h>

int machgate_bind_native_imports(struct machgate_guest_image* image, char* error,
				 size_t error_size);

void machgate_reset_skip_detection(void);
int machgate_skipped_framework_was_called(void);

#endif
