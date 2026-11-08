typedef int (^integer_block)(void);
extern void* _Block_copy(const void* block);
extern void _Block_release(const void* block);

__attribute__((noinline))
static integer_block make_counter(int initial)
{
    __block int value = initial;
    integer_block result = _Block_copy(^{ return ++value; });
    return result;
}

__attribute__((noinline))
static integer_block make_nested(int value)
{
    integer_block inner = ^{ return value; };
    return _Block_copy(^{ return inner() + 1; });
}

__attribute__((noinline))
static void reuse_stack(void)
{
    volatile unsigned long bytes[256];
    for (int index = 0; index < 256; ++index)
        bytes[index] = (unsigned long)index;
}

int main(void)
{
    int captured = 42;
    integer_block stack = ^{ return captured; };
    integer_block heap = _Block_copy(stack);
    if (!heap || heap == stack)
        return 11;
    integer_block retained = _Block_copy(heap);
    _Block_release(heap);
    if (retained() != 42)
        return 12;
    _Block_release(retained);
    integer_block counter = make_counter(40);
    integer_block nested = make_nested(41);
    reuse_stack();
    if (!counter || counter() != 41 || counter() != 42)
        return 13;
    if (!nested || nested() != 42)
        return 14;
    _Block_release(counter);
    _Block_release(nested);
    integer_block global = ^{ return 42; };
    if (_Block_copy(global) != global || global() != 42)
        return 15;
    _Block_release(global);
    if (_Block_copy((void*)0))
        return 16;
    _Block_release((void*)0);
    return 0;
}
