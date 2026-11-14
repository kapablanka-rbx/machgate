typedef unsigned long NSUInteger;
typedef long NSInteger;
typedef struct objc_class* Class;
typedef struct { long first, second, third; } Triple;
#define nil ((id)0)
__attribute__((objc_root_class))
@interface NSObject
+ (id)alloc;
- (id)init;
@end
static int native_init_calls;
@interface NativeBaseChild : NSObject
- (id)init;
@end
@implementation NativeBaseChild
- (id)init {
    if (++native_init_calls > 1) return nil;
    return [super init];
}
@end
@interface NSString : NSObject
+ (id)stringWithUTF8String:(const char*)text;
- (id)initWithUTF8String:(const char*)text;
- (const char*)UTF8String;
@end
@interface NSArray : NSObject
+ (id)array;
- (NSUInteger)count;
- (id)objectAtIndex:(NSUInteger)index;
@end
@interface NSMutableArray : NSArray
- (void)addObject:(id)object;
@end
@interface NSDictionary : NSObject
+ (id)dictionaryWithObjects:(const id*)objects forKeys:(const id*)keys count:(NSUInteger)count;
- (id)objectForKey:(id)key;
@end
@interface NSMutableDictionary : NSDictionary
+ (id)dictionary;
- (void)setObject:(id)object forKey:(id)key;
@end
@interface NSData : NSObject
+ (id)dataWithBytes:(const void*)bytes length:(NSUInteger)length;
- (const void*)bytes;
- (NSUInteger)length;
@end
@interface NSJSONSerialization : NSObject
+ (id)dataWithJSONObject:(id)object options:(NSUInteger)options error:(id*)error;
+ (id)JSONObjectWithData:(id)data options:(NSUInteger)options error:(id*)error;
@end
__attribute__((objc_root_class))
@interface GuestBase { Class isa; id stored; }
+ (id)alloc;
+ (double)classScale:(double)value;
- (long)sum:(long)left right:(long)right;
- (double)scale:(double)value;
- (Triple)triple:(long)value;
- (long)many:(long)a b:(long)b c:(long)c d:(long)d e:(long)e f:(long)f g:(long)g;
@end
@implementation GuestBase
+ (double)classScale:(double)value { return value * 2.0; }
- (long)sum:(long)left right:(long)right { return left + right; }
- (double)scale:(double)value { return value * 2.0; }
- (Triple)triple:(long)value { return (Triple){value, value + 1, value + 2}; }
- (long)many:(long)a b:(long)b c:(long)c d:(long)d e:(long)e f:(long)f g:(long)g { return a+b+c+d+e+f+g; }
@end
@interface GuestChild : GuestBase
- (long)sum:(long)left right:(long)right;
- (double)scale:(double)value;
- (Triple)triple:(long)value;
@end
@implementation GuestChild
- (long)sum:(long)left right:(long)right { return [super sum:left right:right] + 1; }
- (double)scale:(double)value { return [super scale:value] + 1.0; }
- (Triple)triple:(long)value { return [super triple:value]; }
@end
extern id objc_initWeak(id*, id);
extern id objc_loadWeakRetained(id*);
extern void objc_copyWeak(id*, id*);
extern void objc_destroyWeak(id*);
extern void objc_storeStrong(id*, id);
extern id objc_getProperty(id, void*, unsigned long, int);
extern void objc_setProperty_atomic(id, void*, id, unsigned long);
extern id objc_retain(id);
extern Class objc_getClass(const char*);
extern void objc_release(id);
extern void* _Block_copy(const void*);
extern id NSSearchPathForDirectoriesInDomains(unsigned, unsigned, unsigned);
extern unsigned NSStartSearchPathEnumeration(unsigned, unsigned);
extern unsigned NSGetNextSearchPathEnumeration(unsigned, char*);
extern id NSTemporaryDirectory(void);
extern id NSHomeDirectory(void);
static int equal(const char* left, const char* right) {
    if (!left || !right) return 0;
    while (*left && *left == *right) { ++left; ++right; }
    return *left == *right;
}
int main(void) {
    NativeBaseChild* nativeChild = [NativeBaseChild alloc];
    if (!nativeChild || [nativeChild init] != nativeChild || native_init_calls != 1) return 41;
    Class lookedUp = objc_getClass("GuestChild");
    if (!lookedUp || [[lookedUp alloc] sum:20 right:21] != 42) return 40;
    id first = [NSString stringWithUTF8String:"first"];
    id second = [NSString stringWithUTF8String:"second"];
    NSMutableArray* array = [NSMutableArray array];
    [array addObject:first]; [array addObject:second];
    if ([array count] != 2 || !equal([[array objectAtIndex:1] UTF8String], "second")) return 11;
    for (int index = 0; index < 70; ++index) [array addObject:first];
    if ([array count] != 72 || [array objectAtIndex:70] != first) return 27;
    id keys[] = {first}; id objects[] = {second};
    NSDictionary* dictionary = [NSDictionary dictionaryWithObjects:objects forKeys:keys count:1];
    if (!equal([[dictionary objectForKey:first] UTF8String], "second")) return 12;
    NSMutableDictionary* mutable = [NSMutableDictionary dictionary];
    [mutable setObject:second forKey:first];
    if (!equal([[mutable objectForKey:first] UTF8String], "second")) return 13;
    for (int index = 0; index < 16; ++index) {
        char key[] = {'k', (char)('a' + index), 0};
        [mutable setObject:second forKey:[NSString stringWithUTF8String:key]];
    }
    if (!equal([[mutable objectForKey:[NSString stringWithUTF8String:"kp"]] UTF8String], "second")) return 28;
    id error = nil;
    NSData* data = [NSJSONSerialization dataWithJSONObject:dictionary options:0 error:&error];
    if (!data || error || ![data length]) return 14;
    NSDictionary* decoded = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
    if (!equal([[decoded objectForKey:first] UTF8String], "second")) return 15;
    const char raw[] = {'a', 0, 'b'};
    NSData* rawData = [NSData dataWithBytes:raw length:sizeof(raw)];
    const char* bytes = [rawData bytes];
    if ([rawData length] != 3 || !bytes || bytes[0] != 'a' || bytes[1] != 0 || bytes[2] != 'b') return 16;
    id weak = nil, copied = nil, strong = nil;
    objc_initWeak(&weak, first); objc_copyWeak(&copied, &weak);
    if (objc_loadWeakRetained(&copied) != first) return 17;
    objc_destroyWeak(&weak); if (weak) return 18;
    objc_storeStrong(&strong, second); if (strong != second) return 19;
    GuestChild* guest = [GuestChild alloc];
    if ([guest sum:20 right:21] != 42) return 20;
    if ([GuestChild classScale:21.0] != 42.0) return 30;
    if ([guest scale:20.5] != 42.0) return 21;
    Triple triple = [guest triple:40];
    if (triple.first != 40 || triple.second != 41 || triple.third != 42) return 22;
    if ([guest many:1 b:2 c:3 d:4 e:5 f:6 g:21] != 42) return 23;
    objc_setProperty_atomic(guest, 0, first, sizeof(void*));
    if (objc_getProperty(guest, 0, sizeof(void*), 1) != first) return 24;
    NSArray* paths = NSSearchPathForDirectoriesInDomains(5, 1, 1);
    if ([paths count] != 1 || ![[paths objectAtIndex:0] UTF8String]) return 25;
    char path[1024]; unsigned state = NSStartSearchPathEnumeration(5, 1);
    state = NSGetNextSearchPathEnumeration(state, path);
    if (!state || !path[0] || NSGetNextSearchPathEnumeration(state, path) != 0) return 26;
    id owned = [[NSString alloc] initWithUTF8String:"temporary"];
    id weakOwned = nil;
    objc_initWeak(&weakOwned, owned);
    objc_release(owned);
    if (weakOwned) return 29;
    objc_destroyWeak(&weakOwned);
    id captured = [[NSString alloc] initWithUTF8String:"captured"];
    id weakCaptured = nil;
    objc_initWeak(&weakCaptured, captured);
    id (^capturedBlock)(void) = _Block_copy(^{ return captured; });
    objc_release(captured);
    if (!weakCaptured || !equal([capturedBlock() UTF8String], "captured")) return 42;
    objc_release(capturedBlock);
    if (weakCaptured) return 43;
    objc_destroyWeak(&weakCaptured);
    objc_destroyWeak(&copied);
    objc_storeStrong(&strong, nil);
    id tmpDir = NSTemporaryDirectory();
    if (!tmpDir || ![tmpDir UTF8String] || ![tmpDir UTF8String][0]) return 50;
    id homeDir = NSHomeDirectory();
    if (!homeDir || ![homeDir UTF8String] || ![homeDir UTF8String][0]) return 51;
    Class nsfm = objc_getClass("NSFileManager");
    if (!nsfm) return 52;
    id fm = [nsfm defaultManager];
    if (!fm) return 52;
    id testDirName = [NSString stringWithUTF8String:"machgate_test_dir"];
    id testDir = [tmpDir stringByAppendingPathComponent:testDirName];
    if (![fm createDirectoryAtPath:testDir withIntermediateDirectories:(id)1 attributes:nil error:nil]) return 53;
    id fileName = [NSString stringWithUTF8String:"hello.txt"];
    if (![fm createFileAtPath:[testDir stringByAppendingPathComponent:fileName] contents:nil attributes:nil]) return 54;
    id contents = [fm contentsOfDirectoryAtPath:testDir error:nil];
    if (!contents || [contents count] != 1) return 55;
    if (![fm removeItemAtPath:testDir error:nil]) return 56;
    if ([fm fileExistsAtPath:testDir]) return 57;
    return 0;
}
