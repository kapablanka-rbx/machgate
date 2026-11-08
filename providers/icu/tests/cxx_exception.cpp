extern "C" int u_toupper(int character);

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

int main()
{
    if (u_toupper(0x00e4) != 0x00c4)
        return 41;

    int result = 42;
    try {
        throw_derived(17);
    } catch (const std::exception&) {
        result = 0;
    } catch (...) {
        result = 44;
    }

    if (u_toupper(0x03c3) != 0x03a3)
        return 45;
    return result;
}
