

namespace std {
class exception {
public:
    virtual ~exception() noexcept;
    virtual const char* what() const noexcept;
};
}

struct BaseException : std::exception {
    int value;
};

struct DerivedException : BaseException {
};

__attribute__((noinline)) static void throw_derived(int value)
{
    DerivedException exception;
    exception.value = value;
    throw exception;
}

extern "C" void objc_frame();
extern "C" int cleanup_count;
extern "C" void throw_cpp() { throw_derived(17); }

int main()
{

    int result = 42;
    try {
        objc_frame();
    } catch (const std::exception&) {
        result = 0;
    } catch (...) {
        result = 44;
    }

    return result == 0 && cleanup_count == 1 ? 0 : 46;
}
