#include "keychain_compat.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef const void* CFDataRef;
typedef const void* CFDictionaryRef;
typedef long CFIndex;
typedef unsigned long CFTypeID;

extern const void* kCFAllocatorDefault;
extern const void* kSecClass;
extern CFDataRef CFDataCreate(const void* allocator, const unsigned char* bytes,
	CFIndex length);
extern const unsigned char* CFDataGetBytePtr(CFDataRef data);
extern CFIndex CFDataGetLength(CFDataRef data);
extern const void* CFDictionaryGetValue(CFDictionaryRef dictionary,
	const void* key);

enum {
	MACHGATE_SEC_SUCCESS = 0,
	MACHGATE_SEC_PARAM = -50,
	MACHGATE_SEC_ALLOC = -108,
	MACHGATE_SEC_DUPLICATE = -25299,
	MACHGATE_SEC_NOT_FOUND = -25300,
};

struct machgate_keychain_entry {
	struct machgate_keychain_entry* next;
	char* account;
	char* service;
	unsigned char* data;
	size_t data_length;
};

static pthread_mutex_t keychain_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct machgate_keychain_entry* keychain_entries;
static int foundation_lookup_attempted;
static int (*foundation_dictionary_lookup)(const void*, const void*,
	const char**, const void**, size_t*);

static const char keychain_class_generic_password[] = "kSecClassGenericPassword";
static const char keychain_attr_account[] = "kSecAttrAccount";
static const char keychain_attr_service[] = "kSecAttrService";
static const char keychain_attr_accessible[] = "kSecAttrAccessible";
static const char keychain_accessible_after_first_unlock[] =
	"kSecAttrAccessibleAfterFirstUnlock";
static const char keychain_value_data[] = "kSecValueData";
static const char keychain_return_data[] = "kSecReturnData";

const void* kSecClassGenericPassword = keychain_class_generic_password;
const void* kSecAttrAccount = keychain_attr_account;
const void* kSecAttrService = keychain_attr_service;
const void* kSecAttrAccessible = keychain_attr_accessible;
const void* kSecAttrAccessibleAfterFirstUnlock =
	keychain_accessible_after_first_unlock;
const void* kSecValueData = keychain_value_data;
const void* kSecReturnData = keychain_return_data;

static char* copy_string(const char* string)
{
	size_t length = strlen(string) + 1;
	char* result = malloc(length);
	if (result)
		memcpy(result, string, length);
	return result;
}

static void free_entry(struct machgate_keychain_entry* entry)
{
	free(entry->account);
	free(entry->service);
	free(entry->data);
	free(entry);
}

static int strings_equal(const char* left, const char* right)
{
	return left && right && strcmp(left, right) == 0;
}

static int entry_matches(const struct machgate_keychain_entry* entry,
	const char* account, const char* service)
{
	if (account && !strings_equal(entry->account, account))
		return 0;
	if (service && !strings_equal(entry->service, service))
		return 0;
	return 1;
}

static void initialize_foundation_lookup(void)
{
	if (foundation_lookup_attempted)
		return;
	void* handle = dlopen("libmachgate_foundation.so", RTLD_NOW | RTLD_NOLOAD);
	if (handle)
		foundation_dictionary_lookup = dlsym(handle,
			"machgate_keychain_dictionary_lookup_v1");
	foundation_lookup_attempted = 1;
}

extern CFTypeID CFDictionaryGetTypeID(void);
extern CFTypeID CFGetTypeID(const void* object);

static int is_cf_dictionary(const void* dictionary)
{
	if (!dictionary)
		return 0;
	CFTypeID dictionary_type = CFDictionaryGetTypeID();
	return CFGetTypeID(dictionary) == dictionary_type &&
		dictionary_type != 0;
}

static int lookup_foundation_value(const void* dictionary, const void* key,
	const char** string_value, const void** data_bytes, size_t* data_length)
{
	if (foundation_dictionary_lookup)
		return foundation_dictionary_lookup(dictionary, key, string_value,
			data_bytes, data_length);

	const void* value = CFDictionaryGetValue(dictionary, key);
	if (!value)
		return 0;
	if (string_value)
		*string_value = value;
	return 1;
}

static const void* query_attribute(const void* query, const void* key)
{
	if (is_cf_dictionary(query))
		return CFDictionaryGetValue(query, key);
	const char* string_value = NULL;
	if (lookup_foundation_value(query, key, &string_value, NULL, NULL))
		return string_value;
	return NULL;
}

static int validate_generic_password_query(const void* query)
{
	if (!query)
		return 0;
	const char* class_name = query_attribute(query, kSecClass);
	return !class_name ||
		strcmp(class_name, keychain_class_generic_password) == 0;
}

static int read_data_attribute(const void* dictionary, const unsigned char** bytes,
	size_t* length)
{
	*bytes = NULL;
	*length = 0;
	if (is_cf_dictionary(dictionary)) {
		const void* data = CFDictionaryGetValue(dictionary, kSecValueData);
		if (!data)
			return 0;
		*bytes = CFDataGetBytePtr(data);
		CFIndex data_length = CFDataGetLength(data);
		if (data_length < 0 || (!*bytes && data_length > 0))
			return 0;
		*length = (size_t)data_length;
		return 1;
	}
	const void* foundation_bytes = NULL;
	if (!lookup_foundation_value(dictionary, kSecValueData, NULL,
	    &foundation_bytes, length))
		return 0;
	*bytes = foundation_bytes;
	return *bytes || *length == 0;
}

static int32_t add_locked(const void* attributes, const void** result)
{
	if (result)
		*result = NULL;
	if (!validate_generic_password_query(attributes))
		return MACHGATE_SEC_PARAM;

	const char* account = query_attribute(attributes, kSecAttrAccount);
	const char* service = query_attribute(attributes, kSecAttrService);
	const unsigned char* data_bytes;
	size_t data_length;
	if (!read_data_attribute(attributes, &data_bytes, &data_length))
		return MACHGATE_SEC_PARAM;

	for (struct machgate_keychain_entry* entry = keychain_entries; entry;
	     entry = entry->next) {
		if (entry_matches(entry, account, service))
			return MACHGATE_SEC_DUPLICATE;
	}

	struct machgate_keychain_entry* entry = calloc(1, sizeof(*entry));
	if (!entry)
		return MACHGATE_SEC_ALLOC;
	if (account) {
		entry->account = copy_string(account);
		if (!entry->account) {
			free_entry(entry);
			return MACHGATE_SEC_ALLOC;
		}
	}
	if (service) {
		entry->service = copy_string(service);
		if (!entry->service) {
			free_entry(entry);
			return MACHGATE_SEC_ALLOC;
		}
	}
	if (data_length) {
		entry->data = malloc(data_length);
		if (!entry->data) {
			free_entry(entry);
			return MACHGATE_SEC_ALLOC;
		}
		memcpy(entry->data, data_bytes, data_length);
	}
	entry->data_length = data_length;
	entry->next = keychain_entries;
	keychain_entries = entry;
	return MACHGATE_SEC_SUCCESS;
}

static int32_t copy_matching_locked(const void* query, const void** result)
{
	if (!result)
		return MACHGATE_SEC_PARAM;
	*result = NULL;
	if (!validate_generic_password_query(query))
		return MACHGATE_SEC_PARAM;

	const char* account = query_attribute(query, kSecAttrAccount);
	const char* service = query_attribute(query, kSecAttrService);
	for (struct machgate_keychain_entry* entry = keychain_entries; entry;
	     entry = entry->next) {
		if (!entry_matches(entry, account, service))
			continue;
		if (!is_cf_dictionary(query) && foundation_dictionary_lookup) {
			void* (*data_create)(const void*, size_t) = dlsym(
				RTLD_DEFAULT, "foundation_archive_data_create");
			if (data_create) {
				*result = data_create(entry->data, entry->data_length);
				return *result ? MACHGATE_SEC_SUCCESS : MACHGATE_SEC_ALLOC;
			}
		}
		CFDataRef data = CFDataCreate(kCFAllocatorDefault, entry->data,
			(CFIndex)entry->data_length);
		if (!data)
			return MACHGATE_SEC_ALLOC;
		*result = data;
		return MACHGATE_SEC_SUCCESS;
	}
	return MACHGATE_SEC_NOT_FOUND;
}

static int32_t update_locked(const void* query, const void* attributes_to_update)
{
	if (!validate_generic_password_query(query) ||
	    !validate_generic_password_query(attributes_to_update))
		return MACHGATE_SEC_PARAM;

	const char* account = query_attribute(query, kSecAttrAccount);
	const char* service = query_attribute(query, kSecAttrService);
	const unsigned char* data_bytes;
	size_t data_length;
	if (!read_data_attribute(attributes_to_update, &data_bytes, &data_length))
		return MACHGATE_SEC_PARAM;

	for (struct machgate_keychain_entry* entry = keychain_entries; entry;
	     entry = entry->next) {
		if (!entry_matches(entry, account, service))
			continue;
		unsigned char* replacement = NULL;
		if (data_length) {
			replacement = malloc(data_length);
			if (!replacement)
				return MACHGATE_SEC_ALLOC;
			memcpy(replacement, data_bytes, data_length);
		}
		free(entry->data);
		entry->data = replacement;
		entry->data_length = data_length;
		return MACHGATE_SEC_SUCCESS;
	}
	return MACHGATE_SEC_NOT_FOUND;
}

static int32_t delete_locked(const void* query)
{
	if (!validate_generic_password_query(query))
		return MACHGATE_SEC_PARAM;
	const char* account = query_attribute(query, kSecAttrAccount);
	const char* service = query_attribute(query, kSecAttrService);

	struct machgate_keychain_entry** link = &keychain_entries;
	while (*link) {
		struct machgate_keychain_entry* entry = *link;
		if (!entry_matches(entry, account, service)) {
			link = &entry->next;
			continue;
		}
		*link = entry->next;
		free_entry(entry);
		return MACHGATE_SEC_SUCCESS;
	}
	return MACHGATE_SEC_NOT_FOUND;
}

int32_t machgate_keychain_add(const void* attributes, const void** result)
{
	initialize_foundation_lookup();
	pthread_mutex_lock(&keychain_mutex);
	int32_t result_status = add_locked(attributes, result);
	pthread_mutex_unlock(&keychain_mutex);
	return result_status;
}

int32_t machgate_keychain_copy_matching(const void* query, const void** result)
{
	initialize_foundation_lookup();
	pthread_mutex_lock(&keychain_mutex);
	int32_t result_status = copy_matching_locked(query, result);
	pthread_mutex_unlock(&keychain_mutex);
	return result_status;
}

int32_t machgate_keychain_update(const void* query,
	const void* attributes_to_update)
{
	initialize_foundation_lookup();
	pthread_mutex_lock(&keychain_mutex);
	int32_t result_status = update_locked(query, attributes_to_update);
	pthread_mutex_unlock(&keychain_mutex);
	return result_status;
}

int32_t machgate_keychain_delete(const void* query)
{
	initialize_foundation_lookup();
	pthread_mutex_lock(&keychain_mutex);
	int32_t result_status = delete_locked(query);
	pthread_mutex_unlock(&keychain_mutex);
	return result_status;
}
