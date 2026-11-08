typedef unsigned long NSUInteger;
typedef unsigned short unichar;
typedef struct { long majorVersion, minorVersion, patchVersion; } NSOperatingSystemVersion;

__attribute__((objc_root_class))
@interface NSObject
+ (id)alloc;
- (id)init;
- (id)retain;
- (void)release;
@end

@interface NSProcessInfo : NSObject
+ (id)processInfo;
- (NSOperatingSystemVersion)operatingSystemVersion;
@end

@interface NSString : NSObject
+ (id)stringWithFormat:(NSString*)format, ...;
+ (id)stringWithUTF8String:(const char*)text;
+ (id)stringWithCharacters:(const unichar*)characters length:(NSUInteger)length;
- (id)initWithUTF8String:(const char*)text;
- (id)initWithBytes:(const void*)bytes length:(NSUInteger)length encoding:(NSUInteger)encoding;
- (const char*)cStringUsingEncoding:(NSUInteger)encoding;
- (id)dataUsingEncoding:(NSUInteger)encoding;
- (id)stringByAppendingString:(NSString*)other;
- (signed char)hasPrefix:(NSString*)prefix;
- (signed char)isEqualToString:(NSString*)other;
- (id)substringFromIndex:(NSUInteger)index;
- (id)substringToIndex:(NSUInteger)index;
- (const char*)UTF8String;
- (NSUInteger)length;
- (unichar)characterAtIndex:(NSUInteger)index;
- (long)integerValue;
- (double)doubleValue;
- (id)copy;
- (id)mutableCopy;
@end

@interface NSMutableString : NSString
+ (id)stringWithString:(NSString*)string;
- (void)appendString:(NSString*)string;
- (void)appendFormat:(NSString*)format, ...;
@end

@interface NSData : NSObject
- (const void*)bytes;
- (NSUInteger)length;
@end

extern void* objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void*);

static int equal(const char* left, const char* right)
{
	if (!left || !right)
		return 0;
	while (*left && *left == *right) {
		left++;
		right++;
	}
	return *left == *right;
}

int main(void)
{
	void* pool = objc_autoreleasePoolPush();
	NSProcessInfo* process = [NSProcessInfo processInfo];
	if (!process || process != [NSProcessInfo processInfo])
		return 10;
	volatile NSOperatingSystemVersion version = [process operatingSystemVersion];
	if (version.majorVersion != 15 || version.minorVersion != 0 || version.patchVersion != 0)
		return 11;
	NSString* version_string = [NSString stringWithFormat:@"%ld.%ld.%ld",
		version.majorVersion, version.minorVersion, version.patchVersion];
	if (!equal([version_string UTF8String], "15.0.0"))
		return 12;
	NSString* mixed = [NSString stringWithFormat:@"%@ %d %llu %s %.2f %%",
		@"value", -17, 4294967303ULL, "text", 1.25];
	if (!equal([mixed UTF8String], "value -17 4294967303 text 1.25 %"))
		return 13;
	NSString* unicode = @"h\u00e9\U0001f600";
	if (!equal([unicode UTF8String], "h\xc3\xa9\xf0\x9f\x98\x80") || [unicode length] != 4)
		return 14;
	if ([unicode characterAtIndex:2] != 0xd83d || [unicode characterAtIndex:3] != 0xde00)
		return 15;
	const unichar units[] = {0x68, 0xe9, 0xd83d, 0xde00};
	NSString* converted = [NSString stringWithCharacters:units length:4];
	if (!equal([converted UTF8String], [unicode UTF8String]) || [converted length] != 4)
		return 16;
	NSString* copied = [[NSString alloc] initWithUTF8String:[unicode UTF8String]];
	if (!equal([copied UTF8String], [unicode UTF8String]))
		return 17;
	if ([@"-12345678901" integerValue] != -12345678901L || [@"1.25" doubleValue] != 1.25)
		return 18;
	if ([NSString stringWithUTF8String:"\xc0\xaf"] != (id)0)
		return 19;
	const unichar invalid[] = {0xd800};
	if ([NSString stringWithCharacters:invalid length:1] != (id)0)
		return 20;
	NSString* appended = [@"h\u00e9" stringByAppendingString:@"\U0001f600"];
	if (![appended isEqualToString:unicode] || ![appended hasPrefix:@"h\u00e9"] ||
	    [appended hasPrefix:@"hello"] || ![appended hasPrefix:@""])
		return 21;
	if (![[appended substringFromIndex:2] isEqualToString:@"\U0001f600"] ||
	    ![[appended substringToIndex:2] isEqualToString:@"h\u00e9"])
		return 22;
	const unsigned char latin[] = {'c', 'a', 'f', 0xe9};
	NSString* latin_string = [[NSString alloc] initWithBytes:latin length:4 encoding:5];
	if (![latin_string isEqualToString:@"caf\u00e9"] ||
	    !equal([latin_string cStringUsingEncoding:5], "caf\xe9") ||
	    [latin_string cStringUsingEncoding:1] != (void*)0)
		return 23;
	if (!equal([unicode cStringUsingEncoding:4], [unicode UTF8String]))
		return 24;
	const unsigned char embedded[] = {'a', 0, 'b'};
	NSString* embedded_string = [[NSString alloc] initWithBytes:embedded length:3 encoding:4];
	if ([embedded_string length] != 3 || [embedded_string characterAtIndex:2] != 'b' ||
	    [embedded_string isEqualToString:@"a"])
		return 25;
	NSData* encoded_data = [embedded_string dataUsingEncoding:4];
	const unsigned char* encoded_bytes = [encoded_data bytes];
	if ([encoded_data length] != 3 || !encoded_bytes || encoded_bytes[1] != 0 || encoded_bytes[2] != 'b')
		return 26;
	NSData* latin_data = [latin_string dataUsingEncoding:5];
	encoded_bytes = [latin_data bytes];
	if ([latin_data length] != 4 || !encoded_bytes || encoded_bytes[3] != 0xe9 ||
	    [latin_string dataUsingEncoding:1] != (id)0)
		return 27;
	NSMutableString* mutable = [NSMutableString stringWithString:@"caf\u00e9"];
	if (!equal([mutable cStringUsingEncoding:5], "caf\xe9"))
		return 28;
	NSString* snapshot = [mutable copy];
	[mutable appendFormat:@"/%@:%d:%.2f", @"value", 17, 2.25];
	[mutable appendString:@"\U0001f600"];
	if (![mutable isEqualToString:@"caf\u00e9/value:17:2.25\U0001f600"] ||
	    ![snapshot isEqualToString:@"caf\u00e9"] || [mutable length] != 20)
		return 29;
	NSMutableString* independent = [snapshot mutableCopy];
	[independent appendString:independent];
	if (![independent isEqualToString:@"caf\u00e9caf\u00e9"] ||
	    !equal([independent cStringUsingEncoding:5], "caf\xe9" "caf\xe9") ||
	    ![snapshot isEqualToString:@"caf\u00e9"])
		return 30;
	[independent release];
	[snapshot release];
	[embedded_string release];
	[latin_string release];
	[copied release];
	objc_autoreleasePoolPop(pool);
	return 0;
}
