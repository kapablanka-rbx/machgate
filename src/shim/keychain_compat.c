#include "keychain_compat.h"

enum {
	MACHGATE_SEC_NOT_AVAILABLE = -25291,
};

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

int32_t machgate_keychain_add(const void* attributes, const void** result)
{
	(void)attributes;
	if (result)
		*result = NULL;
	return MACHGATE_SEC_NOT_AVAILABLE;
}

int32_t machgate_keychain_copy_matching(const void* query, const void** result)
{
	(void)query;
	if (result)
		*result = NULL;
	return MACHGATE_SEC_NOT_AVAILABLE;
}

int32_t machgate_keychain_update(const void* query,
	const void* attributes_to_update)
{
	(void)query;
	(void)attributes_to_update;
	return MACHGATE_SEC_NOT_AVAILABLE;
}

int32_t machgate_keychain_delete(const void* query)
{
	(void)query;
	return MACHGATE_SEC_NOT_AVAILABLE;
}
