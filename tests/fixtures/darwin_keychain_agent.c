typedef const void* CFTypeRef;
typedef const void* CFDictionaryRef;
typedef const void* CFDataRef;
extern const void* kCFAllocatorDefault;
extern const void* kSecClass;
extern const void* kSecClassGenericPassword;
extern const void* kSecAttrAccount;
extern const void* kSecAttrAccessible;
extern const void* kSecAttrAccessibleAfterFirstUnlock;
extern const void* kSecValueData;
extern const char kCFTypeDictionaryKeyCallBacks;
extern const char kCFTypeDictionaryValueCallBacks;
extern CFDictionaryRef CFDictionaryCreate(const void*, const void**,
	const void**, long, const void*, const void*);
extern CFDataRef CFDataCreate(const void*, const unsigned char*, long);
extern int SecItemAdd(CFDictionaryRef, CFTypeRef*);
extern int SecItemCopyMatching(CFDictionaryRef, CFTypeRef*);
extern int SecItemUpdate(CFDictionaryRef, CFDictionaryRef);
extern int SecItemDelete(CFDictionaryRef);
extern int printf(const char*, ...);

int main(void)
{
	const unsigned char payload[] = "synthetic fixture value";
	CFDataRef data = CFDataCreate(kCFAllocatorDefault, payload,
		sizeof(payload) - 1);
	const void* keys[] = {
		kSecClass, kSecAttrAccount, kSecValueData, kSecAttrAccessible,
	};
	const void* values[] = {
		kSecClassGenericPassword, "machgate-unavailable-fixture", data,
		kSecAttrAccessibleAfterFirstUnlock,
	};
	CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys,
		values, 4, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	for (int iteration = 0; iteration < 2; ++iteration) {
		CFTypeRef result = (void*)1;
		if (SecItemAdd(query, &result) != -25291 || result)
			return 1;
		result = (void*)1;
		if (SecItemCopyMatching(query, &result) != -25291 || result)
			return 2;
		if (SecItemUpdate(query, query) != -25291)
			return 3;
		if (SecItemDelete(query) != -25291)
			return 4;
	}
	printf("unavailable keychain rejects protected-storage operations\n");
	return 0;
}
