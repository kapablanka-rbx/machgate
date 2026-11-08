#include "../runtime.h"
#include "../../string_value.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void* send(void* receiver, const char* selector, uintptr_t first, uintptr_t second, uintptr_t third)
{
    uintptr_t arguments[6] = {first, second, third};
    uintptr_t result = 0;
    double floating = 0;
    assert(foundation_archive_dispatch(receiver, selector, arguments, NULL, NULL, 0, &result, &floating));
    return (void*)result;
}

int main(void)
{
    void* first = foundation_string_create("first", 5, "UTF-8");
    void* second = foundation_string_create("second", 6, "UTF-8");
    void* array = send(foundation_archive_get_class("NSMutableArray"), "array", 0, 0, 0);
    assert(array);
    for (int index = 0; index < 72; ++index)
        send(array, "addObject:", (uintptr_t)first, 0, 0);
    assert((uintptr_t)send(array, "count", 0, 0, 0) == 72);
    assert(send(array, "objectAtIndex:", 70, 0, 0) == first);
    void* dictionary = send(foundation_archive_get_class("NSMutableDictionary"), "dictionary", 0, 0, 0);
    for (int index = 0; index < 16; ++index) {
        char text[] = {'k', (char)('a' + index), 0};
        void* key = foundation_string_create(text, 2, "UTF-8");
        send(dictionary, "setObject:forKey:", (uintptr_t)second, (uintptr_t)key, 0);
    }
    void* key = foundation_string_create("kp", 2, "UTF-8");
    assert(send(dictionary, "objectForKey:", (uintptr_t)key, 0, 0) == second);
    void* json = send(foundation_archive_get_class("NSJSONSerialization"), "dataWithJSONObject:options:error:", (uintptr_t)dictionary, 0, 0);
    assert(json);
    const void* bytes = NULL; size_t size = 0;
    assert(foundation_archive_data_resolve(json, &bytes, &size));
    assert(size > 0 && strstr(bytes, "\"kp\":\"second\""));
    void* decoded = send(foundation_archive_get_class("NSJSONSerialization"), "JSONObjectWithData:options:error:", (uintptr_t)json, 0, 0);
    void* value = send(decoded, "objectForKey:", (uintptr_t)key, 0, 0);
    assert(value && strcmp(foundation_archive_string_utf8(value), "second") == 0);
    const char raw[] = {'a', 0, 'b'};
    void* data = send(foundation_archive_get_class("NSData"), "dataWithBytes:length:", (uintptr_t)raw, sizeof(raw), 0);
    assert((uintptr_t)send(data, "length", 0, 0, 0) == sizeof(raw));
    assert(memcmp(send(data, "bytes", 0, 0, 0), raw, sizeof(raw)) == 0);
    puts("Archive native API behavior passed");
    return 0;
}
