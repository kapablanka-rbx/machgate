typedef unsigned long NSUInteger;
__attribute__((objc_root_class))
@interface NSString
+ (id)stringWithUTF8String:(const char*)text;
+ (id)stringWithContentsOfFile:(id)path encoding:(NSUInteger)encoding error:(id*)error;
- (const char*)UTF8String;
@end
@interface NSError
- (id)localizedDescription;
- (id)domain;
- (long)code;
@end

int main(void)
{
    id error = 0;
    id contents = [NSString stringWithContentsOfFile:
        [NSString stringWithUTF8String:"/machgate-missing-file-read-error-test/input"]
        encoding:4 error:&error];
    if (contents || !error)
        return 1;
    const char* description = [[error localizedDescription] UTF8String];
    if (!description || !description[0])
        return 2;
    const char* domain = [[error domain] UTF8String];
    if (!domain || !domain[0] || ![error code])
        return 3;
    return 0;
}
