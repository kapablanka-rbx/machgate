typedef unsigned long NSUInteger;
#define nil ((id)0)
int printf(const char*, ...);
__attribute__((objc_root_class))
@interface NSObject
@end
@interface NSString : NSObject
+ (id)stringWithUTF8String:(const char*)bytes;
- (int)isEqualToString:(id)other;
@end
@interface NSMutableDictionary : NSObject
+ (id)dictionary;
- (void)setObject:(id)value forKey:(id)key;
@end
@interface NSArray : NSObject
- (NSUInteger)count;
- (id)firstObject;
@end
@interface NSURL : NSObject
+ (id)URLWithString:(id)value;
@end
@interface NSHTTPCookie : NSObject
+ (id)cookiesWithResponseHeaderFields:(id)fields forURL:(id)url;
@end
@interface NSHTTPCookieStorage : NSObject
+ (id)sharedHTTPCookieStorage;
- (void)setCookie:(id)cookie;
- (void)setCookies:(id)cookies forURL:(id)url mainDocumentURL:(id)main;
- (id)cookiesForURL:(id)url;
- (void)deleteCookie:(id)cookie;
@end
@interface NSNotification : NSObject
- (id)name;
- (id)object;
@end
@interface NSNotificationCenter : NSObject
+ (id)defaultCenter;
- (id)addObserverForName:(id)name object:(id)object queue:(id)queue usingBlock:(void (^)(id))block;
- (void)removeObserver:(id)observer;
@end
extern id const NSHTTPCookieManagerCookiesChangedNotification;
static int failures;
static void check(int value, const char* name)
{
    if (!value) {
        printf("FAIL %s\n", name);
        failures++;
    }
}
int main(void)
{
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    NSHTTPCookieStorage* storage = [NSHTTPCookieStorage sharedHTTPCookieStorage];
    id url = [NSURL URLWithString:[NSString stringWithUTF8String:"https://cookie-notification.invalid/"]];
    NSMutableDictionary* fields = [NSMutableDictionary dictionary];
    [fields setObject:[NSString stringWithUTF8String:"synthetic=value; Path=/"] forKey:[NSString stringWithUTF8String:"Set-Cookie"]];
    NSArray* cookies = [NSHTTPCookie cookiesWithResponseHeaderFields:fields forURL:url];
    __block unsigned notifications = 0;
    __block unsigned unwanted = 0;
    __block id observer = nil;
    id wrongName = [center addObserverForName:[NSString stringWithUTF8String:"unrelated"] object:storage queue:nil usingBlock:^(id note) { unwanted++; }];
    id wrongObject = [center addObserverForName:NSHTTPCookieManagerCookiesChangedNotification object:fields queue:nil usingBlock:^(id note) { unwanted++; }];
    observer = [center addObserverForName:NSHTTPCookieManagerCookiesChangedNotification object:storage queue:nil usingBlock:^(NSNotification* note) {
        notifications++;
        check([[note name] isEqualToString:NSHTTPCookieManagerCookiesChangedNotification], "notification name");
        check([note object] == storage, "notification object");
        check([[storage cookiesForURL:url] count] == 1, "callback sees committed cookie");
        [center removeObserver:observer];
    }];
    [storage setCookies:cookies forURL:url mainDocumentURL:nil];
    check(notifications == 1, "set sends notification and supports self removal");
    __block unsigned deletions = 0;
    id deletionObserver = [center addObserverForName:NSHTTPCookieManagerCookiesChangedNotification object:storage queue:nil usingBlock:^(id note) {
        deletions++;
        check([[storage cookiesForURL:url] count] == 0, "delete callback sees committed state");
    }];
    [storage deleteCookie:[cookies firstObject]];
    check(deletions == 1, "delete sends notification");
    check(notifications == 1, "removed observer stays removed");
    check(unwanted == 0, "name and object filters");
    [center removeObserver:wrongName];
    [center removeObserver:wrongObject];
    [center removeObserver:deletionObserver];
    __block unsigned single = 0;
    id singleObserver = [center addObserverForName:NSHTTPCookieManagerCookiesChangedNotification object:storage queue:nil usingBlock:^(id note) {
        single++;
        check([[storage cookiesForURL:url] count] == 1, "single callback sees stored cookie");
    }];
    [storage setCookie:[cookies firstObject]];
    check(single == 1, "single setter sends notification");
    [center removeObserver:singleObserver];
    return failures ? 1 : 0;
}
