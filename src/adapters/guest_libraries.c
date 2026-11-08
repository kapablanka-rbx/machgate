#include "ports/guest_libraries.h"

#include "loader.h"
#include "macho_defs.h"
#include "ports/guest_execution.h"
#include "ports/guest_image_view.h"
#include "ports/guest_symbol_lookup.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MACHGATE_MAX_GUEST_LIBRARIES 64u

struct guest_library {
	struct machgate_guest_image* image;
	uint64_t dependencies;
	int initializing;
	int initialized;
};

static struct machgate_guest_image* main_image;
static struct guest_library libraries[MACHGATE_MAX_GUEST_LIBRARIES];
static size_t library_count;

void machgate_release_guest_libraries(void)
{
	machgate_release_guest_symbol_lookup();
	while (library_count)
		machgate_release_guest_image(libraries[--library_count].image);
	memset(libraries, 0, sizeof(libraries));
	main_image = NULL;
}

void machgate_begin_guest_linkage(struct machgate_guest_image* image)
{
	machgate_release_guest_libraries();
	main_image = image;
}

struct machgate_guest_image* machgate_main_guest_image(void)
{
	return main_image;
}

struct machgate_guest_image* machgate_guest_image_at_index(size_t index)
{
	if (index == 0)
		return main_image;
	return index <= library_count ? libraries[index - 1].image : NULL;
}

static void record_dependency(struct machgate_guest_image* importer, size_t dependency)
{
	for (size_t index = 0; index < library_count; index++)
		if (libraries[index].image == importer)
			libraries[index].dependencies |= UINT64_C(1) << dependency;
}

int machgate_open_guest_dependency(struct machgate_guest_image* importer,
	const char* install_name, struct machgate_guest_image** library,
	char* error, size_t error_size)
{
	*library = NULL;
	const char* source_path;
	const char* relative_path;
	if (strncmp(install_name, "@executable_path/", 17) == 0) {
		source_path = machgate_guest_image_path(main_image);
		relative_path = install_name + 17;
	} else if (strncmp(install_name, "@loader_path/", 13) == 0) {
		source_path = machgate_guest_image_path(importer);
		relative_path = install_name + 13;
	} else {
		return 0;
	}
	if (!source_path)
		return 0;
	const char* slash = strrchr(source_path, '/');
	if (!slash)
		return 0;
	char candidate[PATH_MAX];
	int result = snprintf(candidate, sizeof(candidate), "%.*s/%s",
	                      (int)(slash - source_path), source_path, relative_path);
	if (result < 0 || (size_t)result >= sizeof(candidate)) {
		snprintf(error, error_size, "guest dependency path is too long");
		return -1;
	}
	char resolved[PATH_MAX];
	if (!realpath(candidate, resolved)) {
		if (errno == ENOENT || errno == ENOTDIR)
			return 0;
		snprintf(error, error_size, "could not resolve guest dependency: %s", strerror(errno));
		return -1;
	}
	for (size_t index = 0; index < library_count; index++) {
		if (strcmp(machgate_guest_image_path(libraries[index].image), resolved) != 0)
			continue;
		record_dependency(importer, index);
		*library = libraries[index].image;
		return 1;
	}
	if (library_count == MACHGATE_MAX_GUEST_LIBRARIES) {
		snprintf(error, error_size, "too many guest dependency images");
		return -1;
	}
	struct machgate_guest_image* image;
	if (machgate_load_guest_library(resolved, &image, error, error_size) != 0)
		return -1;
	libraries[library_count].image = image;
	record_dependency(importer, library_count);
	library_count++;
	*library = image;
	return 1;
}

int machgate_prepare_guest_library_images(char* error, size_t error_size)
{
	for (size_t index = 0; index < library_count; index++) {
		struct machgate_guest_image* image = libraries[index].image;
		if (machgate_prepare_native_linkage(image, error, error_size) != 0 ||
		    machgate_prepare_guest_unwind(image, error, error_size) != 0 ||
		    machgate_apply_guest_redirections(image, error, error_size) != 0 ||
		    machgate_prepare_darwin_abi(image, error, error_size) != 0 ||
		    machgate_prepare_guest_runtime_image(image, error, error_size) != 0)
			return -1;
		machgate_complete_guest_image_preparation(image);
	}
	return 0;
}

static int initialize_library(size_t index, int argument_count, char** arguments,
	char** environment, char** apple, uintptr_t stack_pointer,
	char* error, size_t error_size)
{
	struct guest_library* library = &libraries[index];
	if (library->initialized || library->initializing)
		return 0;
	library->initializing = 1;
	for (size_t dependency = 0; dependency < library_count; dependency++) {
		if ((library->dependencies & (UINT64_C(1) << dependency)) &&
		    initialize_library(dependency, argument_count, arguments, environment,
		                       apple, stack_pointer, error, error_size) != 0)
			return -1;
	}
	if (machgate_run_guest_initializers(library->image, argument_count, arguments,
	    environment, apple, stack_pointer, error, error_size) != 0)
		return -1;
	library->initialized = 1;
	library->initializing = 0;
	return 0;
}

int machgate_initialize_guest_libraries(int argument_count, char** arguments,
	char** environment, char** apple, uintptr_t stack_pointer,
	char* error, size_t error_size)
{
	for (size_t index = 0; index < library_count; index++)
		if (initialize_library(index, argument_count, arguments, environment,
		    apple, stack_pointer, error, error_size) != 0)
			return -1;
	return 0;
}
