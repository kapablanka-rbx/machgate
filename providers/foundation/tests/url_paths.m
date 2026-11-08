typedef unsigned char BOOL;
__attribute__((objc_root_class))
@interface NSObject
@end
@interface NSString : NSObject
+ (id)stringWithUTF8String:(const char*)text;
- (const char*)UTF8String;
@end
@interface NSURL : NSObject
+ (id)URLWithString:(NSString*)text;
- (id)URLByAppendingPathComponent:(NSString*)component isDirectory:(BOOL)directory;
- (NSString*)absoluteString;
- (NSString*)path;
@end
@interface NSBundle : NSObject
+ (id)mainBundle;
- (NSURL*)resourceURL;
- (NSString*)resourcePath;
- (NSString*)executablePath;
@end

static int equal(const char* left, const char* right)
{
    if (!left || !right) return 0;
    while (*left && *left == *right) { ++left; ++right; }
    return *left == *right;
}

int main(int argc, char** argv)
{
    NSURL* base = [NSURL URLWithString:[NSString stringWithUTF8String:"file:///tmp/assets"]];
    NSURL* directory = [base URLByAppendingPathComponent:[NSString stringWithUTF8String:"ssl"] isDirectory:1];
    if (!equal([[directory absoluteString] UTF8String], "file:///tmp/assets/ssl/")) return 11;
    if (!equal([[directory path] UTF8String], "/tmp/assets/ssl")) return 12;
    NSURL* file = [directory URLByAppendingPathComponent:[NSString stringWithUTF8String:"a b#%;?.txt"] isDirectory:0];
    if (!equal([[file absoluteString] UTF8String], "file:///tmp/assets/ssl/a%20b%23%25%3B%3F.txt")) return 13;
    if (!equal([[file path] UTF8String], "/tmp/assets/ssl/a b#%;?.txt")) return 14;
    base = [NSURL URLWithString:[NSString stringWithUTF8String:"https://example.com/root?query=1#anchor"]];
    file = [base URLByAppendingPathComponent:[NSString stringWithUTF8String:"caf\xc3\xa9"] isDirectory:0];
    if (!equal([[file absoluteString] UTF8String], "https://example.com/root/caf%C3%A9?query=1#anchor")) return 15;
    if (!equal([[file path] UTF8String], "/root/caf\xc3\xa9")) return 16;
    NSBundle* bundle = [NSBundle mainBundle];
    if (argc < 1 || !equal([[bundle executablePath] UTF8String], argv[0])) return 19;
    directory = [[bundle resourceURL] URLByAppendingPathComponent:[NSString stringWithUTF8String:"ssl"] isDirectory:1];
    const char* basePath = [[bundle resourcePath] UTF8String];
    const char* directoryPath = [[directory path] UTF8String];
    if (!basePath || !directoryPath) return 17;
    while (*basePath && *basePath == *directoryPath) { ++basePath; ++directoryPath; }
    if (*basePath || !equal(directoryPath, "/ssl")) return 18;
    return 0;
}
