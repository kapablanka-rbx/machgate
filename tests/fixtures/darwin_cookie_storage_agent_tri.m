typedef unsigned long NSUInteger;
typedef long NSInteger;
typedef struct objc_class* Class;
#define nil ((id)0)

int printf(const char* format, ...);

__attribute__((objc_root_class))
@interface NSObject
+ (id)alloc;
- (id)init;
@end

@interface NSString : NSObject
+ (id)stringWithUTF8String:(const char*)bytes;
- (NSUInteger)length;
- (const char*)UTF8String;
@end

@interface NSArray : NSObject
- (NSUInteger)count;
- (id)objectAtIndex:(NSUInteger)index;
@end

@interface NSDictionary : NSObject
- (id)objectForKey:(id)key;
@end

@interface NSMutableDictionary : NSDictionary
+ (id)dictionary;
- (void)setObject:(id)anObject forKey:(id)aKey;
@end

@interface NSURL : NSObject
+ (id)URLWithString:(id)string;
@end

@interface NSHTTPCookie : NSObject
+ (id)cookiesWithResponseHeaderFields:(id)headerFields forURL:(id)URL;
+ (id)requestHeaderFieldsWithCookies:(id)cookies;
- (id)name;
- (id)value;
@end

static id makeCookiesForUrl(const char* setCookie, const char* url)
{
    NSMutableDictionary* fields = [NSMutableDictionary dictionary];
    [fields setObject:[NSString stringWithUTF8String:setCookie] forKey:[NSString stringWithUTF8String:"Set-Cookie"]];
    return [NSHTTPCookie cookiesWithResponseHeaderFields:fields forURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

static void storeCookies(id storage, id cookies, const char* url)
{
    [storage setCookies:cookies forURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]] mainDocumentURL:nil];
}

static id cookiesForUrl(id storage, const char* url)
{
    return [storage cookiesForURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

static int failures = 0;

static void expectCookieCount(const char* label, id cookies, NSUInteger expected)
{
    NSUInteger actual = [cookies count];
    if (actual != expected)
    {
        printf("FAIL %s: expected %lu cookies got %lu\n", label, (unsigned long)expected, (unsigned long)actual);
        failures++;
    }
}

@interface NSHTTPCookieStorage : NSObject
+ (id)sharedHTTPCookieStorage;
- (id)cookiesForURL:(id)URL;
- (void)setCookies:(id)cookies forURL:(id)URL mainDocumentURL:(id)mainDocumentURL;
- (void)deleteCookie:(id)cookie;
- (id)cookies;
@end

static int stringEquals(id left, const char* right)
{    const char* leftBytes = [left UTF8String];
    if (!leftBytes || !right)
        return 0;
    while (*leftBytes && *right && *leftBytes == *right)
    {
        leftBytes++;
        right++;
    }
    return *leftBytes == '\0' && *right == '\0';
}

static void expectNamedCookie(id cookies, const char* name, const char* value)
{
    NSUInteger count = [cookies count];
    for (NSUInteger index = 0; index < count; index++)
    {
        id cookie = [cookies objectAtIndex:index];
        if (stringEquals([cookie name], name))
        {
            if (!stringEquals([cookie value], value))
            {
                printf("FAIL %s: expected value \"%s\" got \"%s\"\n", name, value, [[cookie value] UTF8String]);
                failures++;
            }
            return;
        }
    }
    printf("FAIL cookie \"%s\" not found in portless-host match\n", name);
    failures++;
}

int main(void)
{
    NSHTTPCookieStorage* storage = [NSHTTPCookieStorage sharedHTTPCookieStorage];
    NSArray* stale = [storage cookies];
    for (NSUInteger index = 0; index < [stale count]; index++)
        [storage deleteCookie:[stale objectAtIndex:index]];
    NSURL* plainUrl = [NSURL URLWithString:[NSString stringWithUTF8String:"http://127.0.0.1/cookie-port-test"]];

    NSArray* cookies = makeCookiesForUrl("SomeCookie=123; path=/", "http://127.0.0.1:5256/__mock/cookie-port-test");
    storeCookies(storage, cookies, "http://127.0.0.1:5256/__mock/cookie-port-test");

    NSArray* matched = cookiesForUrl(storage, "http://127.0.0.1/cookie-port-test");
    if ([matched count] == 0)
    {
        printf("FAIL portless-host-read: no cookies matched\n");
        failures++;
    }
    else
    {
        expectNamedCookie(matched, "SomeCookie", "123");
    }

    NSArray* allBefore = [storage cookies];
    for (NSUInteger index = 0; index < [allBefore count]; index++)
        [storage deleteCookie:[allBefore objectAtIndex:index]];

    NSArray* pathCookies = makeCookiesForUrl("pathCookie=pv; Path=/CookiePath", "https://www.asdf1.com/CookiePath");
    storeCookies(storage, pathCookies, "https://www.asdf1.com/CookiePath");
    expectCookieCount("path-wrong-path", cookiesForUrl(storage, "https://www.asdf1.com/"), 0);
    expectCookieCount("path-exact", cookiesForUrl(storage, "https://www.asdf1.com/CookiePath"), 1);
    expectCookieCount("path-longer", cookiesForUrl(storage, "https://www.asdf1.com/CookiePath/longerpath"), 1);

    NSArray* secureCookies = makeCookiesForUrl("secureCookie=sv; Secure", "https://www.securehost1.com/");
    storeCookies(storage, secureCookies, "https://www.securehost1.com/");
    expectCookieCount("secure-non-https", cookiesForUrl(storage, "http://www.securehost1.com/"), 0);
    expectCookieCount("secure-https", cookiesForUrl(storage, "https://www.securehost1.com/"), 1);

    NSArray* expiredCookies = makeCookiesForUrl("expiredCookie=ev; Expires=Wed, 21 Oct 2015 07:28:00 GMT", "https://www.asdf1.com/");
    storeCookies(storage, expiredCookies, "https://www.asdf1.com/");
    expectCookieCount("expired-date", cookiesForUrl(storage, "https://www.asdf1.com/"), 0);

    NSArray* maxAgeCookies = makeCookiesForUrl("maxAgeCookie=mv; Max-Age=0", "https://www.asdf2.com/");
    storeCookies(storage, maxAgeCookies, "https://www.asdf2.com/");
    expectCookieCount("expired-max-age", cookiesForUrl(storage, "https://www.asdf2.com/"), 0);

    NSArray* allFinal = [storage cookies];
    for (NSUInteger index = 0; index < [allFinal count]; index++)
        [storage deleteCookie:[allFinal objectAtIndex:index]];
    NSArray* afterDelete = cookiesForUrl(storage, "https://www.asdf1.com/CookiePath");
    if ([afterDelete count] != 0)
    {
        printf("FAIL delete-cookie: cookies survived\n");
        failures++;
    }

    if (failures == 0)
        printf("cookie storage port-host behavior passed\n");
    return failures == 0 ? 0 : 1;
}
