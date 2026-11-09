extern "C" long write(int, const void*, unsigned long);
extern "C" void __cxa_throw(void*, void*, void(*)(void*));
extern "C" void* __cxa_allocate_exception(unsigned long);
extern "C" void __cxa_begin_catch();
extern "C" void __cxa_end_catch();
extern "C" void __cxa_rethrow();
extern "C" void* __cxa_current_primary_exception();
extern "C" void __cxa_increment_exception_refcount(void*);
extern "C" void __cxa_decrement_exception_refcount(void*);
extern "C" void __cxa_rethrow_primary_exception(void*);

static void emit(const char* text)
{
    unsigned long length = 0;
    while (text[length])
        length++;
    write(1, text, length);
}

static volatile double sink;

__attribute__((noinline)) static void throw_it()
{
    void* exception = __cxa_allocate_exception(16);
    __cxa_throw(exception, 0, 0);
}

__attribute__((noinline)) static void recurse_to_throw(int depth, double kept)
{
    kept = kept * 1.5 + 0.25;
    sink = kept;
    if (depth > 0) {
        recurse_to_throw(depth - 1, kept);
        return;
    }
    throw_it();
    if (kept < 0.5)
        emit("bad-double-recurse\n");
}

__attribute__((noinline)) static void pair_frame_rethrow()
{
    double kept = sink + 1.0;
    try {
        throw_it();
    } catch (...) {
        emit("inner-caught\n");
        if (kept < 0.5) {
            emit("bad-double-pair\n");
            return;
        }
        sink = kept * 2.0;
        throw;
    }
}

__attribute__((noinline)) static void* capture_exception_ptr()
{
    double kept = sink + 1.0;
    try {
        throw_it();
    } catch (...) {
        void* captured = __cxa_current_primary_exception();
        if (captured)
            __cxa_increment_exception_refcount(captured);
        sink = kept * 3.0;
        return captured;
    }
    return 0;
}

__attribute__((noinline)) static void rethrow_captured(void* captured)
{
    double kept = sink + 1.0;
    sink = kept * 2.0;
    __cxa_rethrow_primary_exception(captured);
}

__attribute__((noinline)) static int scenario_deep_chain()
{
    emit("deep-chain\n");
    try {
        recurse_to_throw(7, sink + 1.0);
        emit("bad-no-exc-deep\n");
        return 0;
    } catch (...) {
        emit("deep-caught\n");
        return 1;
    }
}

__attribute__((noinline)) static int scenario_pair_rethrow()
{
    emit("pair-rethrow\n");
    try {
        pair_frame_rethrow();
        emit("bad-no-exc-pair\n");
        return 0;
    } catch (...) {
        emit("pair-caught\n");
        return 1;
    }
}

__attribute__((noinline)) static int scenario_exception_ptr()
{
    emit("exception-ptr\n");
    void* captured = capture_exception_ptr();
    if (!captured) {
        emit("bad-capture-failed\n");
        return 0;
    }
    emit("after-capture\n");
    try {
        rethrow_captured(captured);
    } catch (...) {
        emit("rethrown-caught\n");
        __cxa_decrement_exception_refcount(captured);
        return 1;
    }
    emit("bad-no-exc-rethrow\n");
    return 0;
}

int main()
{
    emit("start\n");
    if (!scenario_deep_chain())
        return 3;
    if (!scenario_pair_rethrow())
        return 4;
    if (!scenario_exception_ptr())
        return 5;
    emit("done\n");
    return 42;
}
