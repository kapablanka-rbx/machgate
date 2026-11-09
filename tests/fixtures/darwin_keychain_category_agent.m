typedef unsigned long NSUInteger;
typedef long NSInteger;
typedef struct objc_class* Class;
typedef const void* CFDictionaryRef;
typedef const void* CFTypeRef;
typedef signed int OSStatus;
typedef signed char BOOL;
#define YES ((BOOL)1)
#define NO ((BOOL)0)
#define nil ((id)0)

int printf(const char* format, ...);

static int strings_equal(const char* left, const char* right)
{
    while (*left && *left == *right) {
        left++;
        right++;
    }
    return *left == *right;
}

__attribute__((objc_root_class))
@interface NSObject
+ (id)alloc;
+ (Class)class;
- (id)init;
- (id)copy;
- (id)mutableCopy;
- (id)retain;
- (void)release;
@end

@interface NSString : NSObject
+ (id)stringWithUTF8String:(const char*)bytes;
- (const char*)UTF8String;
@end

@interface NSData : NSObject
@end

@interface NSArray : NSObject
@end

@interface NSDictionary : NSObject
- (id)objectForKey:(id)key;
@end

@interface NSMutableDictionary : NSDictionary
+ (id)dictionary;
- (void)setObject:(id)anObject forKey:(id)aKey;
- (void)removeObjectForKey:(id)aKey;
@end

@interface NSKeyedArchiver : NSObject
+ (id)archivedDataWithRootObject:(id)object
    requiringSecureCoding:(BOOL)requiresSecureCoding error:(id*)error;
@end

@interface NSKeyedUnarchiver : NSObject
+ (id)unarchivedObjectOfClass:(Class)class fromData:(id)data error:(id*)error;
@end

OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef* result);
OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef* result);
OSStatus SecItemDelete(CFDictionaryRef query);

extern const void* kSecClass;
extern const void* kSecClassGenericPassword;
extern const void* kSecAttrAccount;
extern const void* kSecValueData;
extern const void* kSecReturnData;

@interface NSDictionary (KeychainSecureStorage)
- (BOOL)storeToKeychainWithKey:(NSString*)key;
+ (NSDictionary*)dictionaryFromKeychainWithKey:(NSString*)key;
+ (BOOL)deleteFromKeychainWithKey:(NSString*)key;
@end

@implementation NSDictionary (KeychainSecureStorage)
- (BOOL)storeToKeychainWithKey:(NSString*)key
{
    NSData* serialized = [NSKeyedArchiver archivedDataWithRootObject:self
        requiringSecureCoding:YES error:nil];
    [NSDictionary deleteFromKeychainWithKey:key];
    if (!serialized)
        return NO;
    NSMutableDictionary* query = [NSMutableDictionary dictionary];
    [query setObject:key forKey:(NSString*)kSecAttrAccount];
    [query setObject:serialized forKey:(NSString*)kSecValueData];
    [query setObject:(NSString*)kSecClassGenericPassword forKey:(NSString*)kSecClass];
    return SecItemAdd((CFDictionaryRef)query, nil) == 0;
}

+ (NSDictionary*)dictionaryFromKeychainWithKey:(NSString*)key
{
    NSMutableDictionary* query = [NSMutableDictionary dictionary];
    [query setObject:key forKey:(NSString*)kSecAttrAccount];
    [query setObject:(NSString*)kSecClassGenericPassword forKey:(NSString*)kSecClass];
    [query setObject:(NSString*)kSecReturnData forKey:(NSString*)kSecReturnData];
    CFTypeRef serialized = nil;
    if (SecItemCopyMatching((CFDictionaryRef)query, &serialized) != 0)
        return nil;
    return [NSKeyedUnarchiver unarchivedObjectOfClass:[NSDictionary class]
        fromData:(NSData*)serialized error:nil];
}

+ (BOOL)deleteFromKeychainWithKey:(NSString*)key
{
    NSMutableDictionary* query = [NSMutableDictionary dictionary];
    [query setObject:key forKey:(NSString*)kSecAttrAccount];
    [query setObject:(NSString*)kSecClassGenericPassword forKey:(NSString*)kSecClass];
    return SecItemDelete((CFDictionaryRef)query) == 0;
}
@end

static int failures;

static void check(const char* operation, int condition)
{
    if (condition)
        return;
    printf("FAIL %s\n", operation);
    failures++;
}

static BOOL dictionary_round_trip(const char* account, const char* key,
    const char* value)
{
    NSString* storage_key = [NSString stringWithUTF8String:account];
    NSMutableDictionary* storage = [NSMutableDictionary dictionary];
    [storage setObject:[NSString stringWithUTF8String:value]
        forKey:[NSString stringWithUTF8String:key]];
    if (![storage storeToKeychainWithKey:storage_key])
        return NO;
    NSDictionary* loaded = [NSDictionary dictionaryFromKeychainWithKey:storage_key];
    if (!loaded)
        return NO;
    NSString* loaded_value = [loaded objectForKey:
        [NSString stringWithUTF8String:key]];
    return loaded_value && strings_equal([loaded_value UTF8String], value);
}

int main(void)
{
    NSString* account = [NSString stringWithUTF8String:
        "www.roblox.com:guid-root"];
    check("delete missing item",
        [NSDictionary deleteFromKeychainWithKey:account] == NO);

    check("dictionary category round-trip",
        dictionary_round_trip("www.roblox.com:guid-root", "TestKey",
            "UpdatedValue"));
    check("dictionary category round-trip UTF-8",
        dictionary_round_trip("www.roblox.com:guid-\xe6\xa0\xb9\xe9\x91\xb0\xe5\x8c\x99",
            "\xe9\x91\xb0\xe5\x8c\x99", "\xe6\xb8\xac\xe8\xa9\xa6"));

    check("delete stored item",
        [NSDictionary deleteFromKeychainWithKey:account] == YES);
    check("load after delete",
        [NSDictionary dictionaryFromKeychainWithKey:account] == nil);

    if (failures == 0)
        printf("keychain category storage and keyed archive round-trip passed\n");
    return failures == 0 ? 0 : 1;
}
