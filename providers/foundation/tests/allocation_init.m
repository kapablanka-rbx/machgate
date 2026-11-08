typedef unsigned long NSUInteger;
__attribute__((objc_root_class))
@interface NSObject
+ (id)alloc;
+ (Class)class;
@end
@interface NSCharacterSet : NSObject
+ (id)URLPathAllowedCharacterSet;
@end
@class NSString;
@interface NSURLComponents : NSObject
- (id)init;
- (void)setHost:(NSString*)host;
- (id)URL;
@end
extern id objc_alloc_init(Class classObject);
extern Class objc_getClass(const char* name);
@interface NSString : NSObject
+ (id)stringWithUTF8String:(const char*)text;
- (id)initWithBytes:(const void*)bytes length:(NSUInteger)length encoding:(NSUInteger)encoding;
- (id)stringByTrimmingCharactersInSet:(NSCharacterSet*)set;
- (NSUInteger)length;
- (const char*)UTF8String;
@end
static int equal(const char* left, const char* right)
{
    if (!left || !right) return 0;
    while (*left && *left == *right) { ++left; ++right; }
    return *left == *right;
}
int main(void)
{
    if (objc_alloc_init((Class)0)) return 20;
    NSCharacterSet* set = [NSCharacterSet URLPathAllowedCharacterSet];
    NSString* string = [NSString stringWithUTF8String:"RootKey_123-abc"];
    if ([[string stringByTrimmingCharactersInSet:set] length] != 0) return 11;
    string = [[NSString alloc] initWithBytes:"RootKey_123-abc" length:14 encoding:4];
    if ([[string stringByTrimmingCharactersInSet:set] length] != 0) return 12;
    string = [NSString stringWithUTF8String:"abc x yz"];
    if (!equal([[string stringByTrimmingCharactersInSet:set] UTF8String], " x ")) return 13;
    string = [NSString stringWithUTF8String:"?bad?"];
    if (!equal([[string stringByTrimmingCharactersInSet:set] UTF8String], "?bad?")) return 14;
    NSURLComponents* components = objc_alloc_init(objc_getClass("NSURLComponents"));
    if (!components) return 21;
    [components setHost:[NSString stringWithUTF8String:"example.com"]];
    if (![components URL]) return 22;
    return 0;
}
