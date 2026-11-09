#ifndef MACHGATE_SHIM_KEYCHAIN_COMPAT_H
#define MACHGATE_SHIM_KEYCHAIN_COMPAT_H

#include <stddef.h>
#include <stdint.h>

/* Process-local generic-password keychain: strings are copied, values are
 * matched by account/service and returned as CFData or provider NSData. */
int32_t machgate_keychain_add(const void* attributes, const void** result);
int32_t machgate_keychain_copy_matching(const void* query, const void** result);
int32_t machgate_keychain_update(const void* query,
	const void* attributes_to_update);
int32_t machgate_keychain_delete(const void* query);

extern const void* kSecClassGenericPassword;
extern const void* kSecAttrAccount;
extern const void* kSecAttrService;
extern const void* kSecAttrAccessible;
extern const void* kSecAttrAccessibleAfterFirstUnlock;
extern const void* kSecValueData;
extern const void* kSecReturnData;

#endif
