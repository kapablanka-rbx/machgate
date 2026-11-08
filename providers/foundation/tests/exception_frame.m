__attribute__((objc_root_class))
@interface NSException
@end
@implementation NSException
@end

extern void throw_cpp(void);
int cleanup_count;
void objc_frame(void)
{
    @try { throw_cpp(); }
    @catch (NSException* exception) { cleanup_count = 99; }
    @finally { ++cleanup_count; }
}
