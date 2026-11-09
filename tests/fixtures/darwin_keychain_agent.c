typedef __SIZE_TYPE__ size_t;
typedef signed int OSStatus;
#define NULL ((void*)0)
typedef const void* CFAllocatorRef;
typedef const void* CFDataRef;
typedef const void* CFDictionaryRef;
typedef const void* CFTypeRef;
typedef unsigned char uint8_t;
typedef unsigned long uintptr_t;
typedef long CFIndex;

extern const void* kCFAllocatorDefault;
extern const void* kCFBooleanTrue;
extern const char kCFTypeDictionaryKeyCallBacks;
extern const char kCFTypeDictionaryValueCallBacks;
extern const void* kSecClass;
extern const void* kSecClassGenericPassword;
extern const void* kSecAttrAccount;
extern const void* kSecAttrAccessible;
extern const void* kSecAttrAccessibleAfterFirstUnlock;
extern const void* kSecValueData;
extern const void* kSecReturnData;

extern CFDictionaryRef CFDictionaryCreate(CFAllocatorRef allocator,
	const void** keys, const void** values, CFIndex count,
	const void* key_callbacks, const void* value_callbacks);
extern CFDataRef CFDataCreate(CFAllocatorRef allocator, const uint8_t* bytes,
	CFIndex length);
extern const uint8_t* CFDataGetBytePtr(CFDataRef data);
extern CFIndex CFDataGetLength(CFDataRef data);
extern void CFRelease(CFTypeRef object);
extern OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef* result);
extern OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef* result);
extern OSStatus SecItemUpdate(CFDictionaryRef query,
	CFDictionaryRef attributes_to_update);
extern OSStatus SecItemDelete(CFDictionaryRef query);
extern int printf(const char* format, ...);

static int failures;

static void expect_status(const char* operation, OSStatus actual,
	OSStatus expected)
{
	if (actual == expected)
		return;
	printf("FAIL %s: status %d, expected %d\n", operation, actual, expected);
	failures++;
}

static CFDictionaryRef make_dictionary(const void** keys, const void** values,
	CFIndex count)
{
	return CFDictionaryCreate(kCFAllocatorDefault, keys, values, count,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static size_t string_length(const char* string)
{
	const char* cursor = string;
	while (*cursor)
		cursor++;
	return (size_t)(cursor - string);
}

static int bytes_equal(const uint8_t* left, const char* right, size_t length)
{
	for (size_t index = 0; index < length; index++) {
		if (left[index] != (uint8_t)right[index])
			return 0;
	}
	return 1;
}

static void expect_stored_value(const char* account, const char* expected,
	size_t expected_length)
{
	const void* keys[] = { kSecClass, kSecAttrAccount, kSecReturnData };
	const void* values[] = { kSecClassGenericPassword, account, kCFBooleanTrue };
	CFDictionaryRef query = make_dictionary(keys, values, 3);
	CFTypeRef result = NULL;
	OSStatus status = SecItemCopyMatching(query, &result);
	expect_status("copy existing", status, 0);
	if (status != 0)
		return;

	const uint8_t* bytes = CFDataGetBytePtr(result);
	CFIndex length = CFDataGetLength(result);
	if (length != (CFIndex)expected_length ||
	    !bytes_equal(bytes, expected, expected_length)) {
		printf("FAIL copy value for %s: length %ld, expected %lu\n",
			account, length, (unsigned long)expected_length);
		failures++;
	}
	CFRelease(result);
}

static void expect_missing(const char* account)
{
	const void* keys[] = { kSecClass, kSecAttrAccount, kSecReturnData };
	const void* values[] = { kSecClassGenericPassword, account, kCFBooleanTrue };
	CFDictionaryRef query = make_dictionary(keys, values, 3);
	CFTypeRef result = (CFTypeRef)(uintptr_t)1;
	OSStatus status = SecItemCopyMatching(query, &result);
	expect_status("copy missing", status, -25300);
	if (result != NULL) {
		printf("FAIL copy missing result was not cleared\n");
		failures++;
	}
}

static void delete_item(const char* account, OSStatus expected)
{
	const void* keys[] = { kSecClass, kSecAttrAccount, kSecReturnData };
	const void* values[] = { kSecClassGenericPassword, account, kCFBooleanTrue };
	CFDictionaryRef query = make_dictionary(keys, values, 3);
	expect_status("delete", SecItemDelete(query), expected);
}

static void add_item(const char* account, const char* value, size_t length,
	OSStatus expected)
{
	CFDataRef data = CFDataCreate(kCFAllocatorDefault, value, length);
	const void* keys[] = {
		kSecClass, kSecAttrAccount, kSecValueData, kSecAttrAccessible
	};
	const void* values[] = {
		kSecClassGenericPassword, account, data,
		kSecAttrAccessibleAfterFirstUnlock
	};
	CFDictionaryRef attributes = make_dictionary(keys, values, 4);
	expect_status("add", SecItemAdd(attributes, NULL), expected);
}

static void update_item(const char* account, const char* value, size_t length)
{
	const void* query_keys[] = { kSecClass, kSecAttrAccount };
	const void* query_values[] = { kSecClassGenericPassword, account };
	CFDictionaryRef query = make_dictionary(query_keys, query_values, 2);
	CFDataRef data = CFDataCreate(kCFAllocatorDefault, value, length);
	const void* update_keys[] = { kSecValueData };
	const void* update_values[] = { data };
	CFDictionaryRef update = make_dictionary(update_keys, update_values, 1);
	expect_status("update", SecItemUpdate(query, update), 0);
}

static void exercise_round_trip(const char* account, const char* initial,
	size_t initial_length, const char* replacement)
{
	delete_item(account, -25300);
	expect_missing(account);
	add_item(account, initial, initial_length, 0);
	expect_stored_value(account, initial, initial_length);
	add_item(account, replacement, string_length(replacement), -25299);
	update_item(account, replacement, string_length(replacement));
	expect_stored_value(account, replacement, string_length(replacement));
	delete_item(account, 0);
	expect_missing(account);
}

int main(void)
{
	static const char long_value[] =
		"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
		"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
		"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
		"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
		"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
		"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
		"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
		"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
	const char binary_value[] = { 'a', '\0', 'b', (char)0xff };

	exercise_round_trip("www.roblox.com:guid-root:TestKey", long_value,
		sizeof(long_value) - 1, "UpdatedValue");
	exercise_round_trip("www.roblox.com:guid-\xe6\xa0\xb9\xe9\x91\xb0\xe5\x8c\x99:\xe9\x91\xb0\xe5\x8c\x99",
		"\xe6\xb8\xac\xe8\xa9\xa6", 6, "\xe6\x9b\xb4\xe6\x96\xb0");
	exercise_round_trip("binary", binary_value, sizeof(binary_value), "next");

	if (failures == 0)
		printf("keychain generic-password add/copy/update/delete behavior passed\n");
	return failures == 0 ? 0 : 1;
}
