#ifdef TSO_ENABLE_ASSERTS
#ifdef TSO_PLATFORM_WINDOWS
#define TSO_ASSERT(x, ...) {if(!(x)){TSO_ERROR("Assertion Failed: {0}",__VA_ARGS__); __debugbreak();}}
#define TSO_CORE_ASSERT(x, ...){if(!(x)){TSO_CORE_ERROR("Assertion Failed: {0}",__VA_ARGS__); __debugbreak();}}
#elif defined TSO_PLATFORM_MACOSX
#define TSO_ASSERT(x, ...) {if(!(x)){TSO_ERROR("Assertion Failed: {0}",__VA_ARGS__); __builtin_trap();}}
#define TSO_CORE_ASSERT(x, ...){if(!(x)){TSO_CORE_ERROR("Assertion Failed: {0}",__VA_ARGS__); __builtin_trap();}}
#else
#endif
#else
#define SERVER_ASSERT(x, ...) 
#endif // TSO_ENABLE_ASSERTS


namespace Tso {

    template<typename T>
    using Scope = std::unique_ptr<T>;

    template<typename T, typename... Arg>
    constexpr Scope<T> CreateScope(Arg&& ...arg) {
        return std::make_unique<T>(std::forward<Arg>(arg)...);
    }

    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T, typename... Arg>
    constexpr Ref<T> CreateRef(Arg&& ...arg) {
        return std::make_shared<T>(std::forward<Arg>(arg)...);
    }
}