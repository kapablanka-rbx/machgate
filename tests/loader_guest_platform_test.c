#include "loader.h"
#include "ports/guest_image_view.h"

#include <stdio.h>
#include <stdlib.h>

int machgate_adapt_guest_instructions(struct machgate_guest_image* image,
				      char* error, size_t error_size)
{
	(void)image;
	(void)error;
	(void)error_size;
	return 0;
}

static int load_platform(const char* path, int expected, const char* override)
{
	if (override) {
		if (setenv("MACHGATE_GUEST_PLATFORM", override, 1) != 0)
			return 1;
	} else if (unsetenv("MACHGATE_GUEST_PLATFORM") != 0) {
		return 1;
	}

	struct machgate_guest_image* image = NULL;
	char error[256] = {0};
	if (machgate_load_executable(path, &image, error, sizeof(error)) != 0) {
		fprintf(stderr, "%s\n", error);
		return 1;
	}
	int result = machgate_guest_image_platform(image) != expected;
	machgate_unload_guest_image(image);
	return result;
}

static int reject_platform(const char* path, const char* override)
{
	setenv("MACHGATE_GUEST_PLATFORM", override, 1);
	struct machgate_guest_image* image = NULL;
	char error[256] = {0};
	int result = machgate_load_executable(path, &image, error, sizeof(error));
	machgate_unload_guest_image(image);
	return result != 0;
}

int main(int argument_count, char** arguments)
{
	if (argument_count != 3)
		return 1;
	if (load_platform(arguments[1], 1, NULL) != 0)
		return 2;
	if (load_platform(arguments[2], 2, NULL) != 0)
		return 3;
	if (load_platform(arguments[2], 1, "macos") != 0)
		return 4;
	if (load_platform(arguments[1], 2, "ios") != 0)
		return 5;
	if (!reject_platform(arguments[1], "2"))
		return 6;
	return 0;
}
