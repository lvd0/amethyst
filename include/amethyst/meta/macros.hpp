#pragma once

#define AM_DECLARE_COPY(T) \
    T(const T&) noexcept; \
    T& operator =(const T&) noexcept

#define AM_DEFAULT_COPY(T) \
    T(const T&) noexcept = default; \
    T& operator =(const T&) noexcept = default

#define AM_DELETE_COPY(T) \
    T(const T&) noexcept = delete; \
    T& operator =(const T&) noexcept = delete

#define AM_DECLARE_MOVE(T) \
    T(T&&) noexcept; \
    T& operator =(T&&) noexcept

#define AM_DEFAULT_MOVE(T) \
    T(T&&) noexcept = default; \
    T& operator =(T&&) noexcept = default

#define AM_DELETE_MOVE(T) \
    T(T&&) noexcept = delete; \
    T& operator =(T&&) noexcept = delete

#define AM_QUALIFIED_DECLARE_COPY(T, ...) \
    __VA_ARGS__ T(const T&) noexcept; \
    __VA_ARGS__ T& operator =(const T&) noexcept

#define AM_QUALIFIED_DEFAULT_COPY(T, ...) \
    __VA_ARGS__ T(const T&) noexcept = default; \
    __VA_ARGS__ T& operator =(const T&) noexcept = default

#define AM_QUALIFIED_DELETE_COPY(T, ...) \
    __VA_ARGS__ T(const T&) noexcept = delete; \
    __VA_ARGS__ T& operator =(const T&) noexcept = delete

#define AM_QUALIFIED_DECLARE_MOVE(T, ...) \
    __VA_ARGS__ T(T&&) noexcept; \
    __VA_ARGS__ T& operator =(T&&) noexcept

#define AM_QUALIFIED_DEFAULT_MOVE(T, ...) \
    __VA_ARGS__ T(T&&) noexcept = default; \
    __VA_ARGS__ T& operator =(T&&) noexcept = default

#define AM_QUALIFIED_DELETE_MOVE(T, ...) \
    __VA_ARGS__ T(T&&) noexcept = delete; \
    __VA_ARGS__ T& operator =(T&&) noexcept = delete

#define AM_NODISCARD [[nodiscard]]
#define AM_NORETURN [[noreturn]]

#if defined(_MSC_VER)
    #define AM_CPP_VERSION _MSVC_LANG
    #define AM_UNREACHABLE() __assume(false)
    #define AM_SIGNATURE() __FUNCSIG__
#else
    #define AM_CPP_VERSION __cplusplus
    #define AM_UNREACHABLE() __builtin_unreachable()
    #define AM_SIGNATURE() __PRETTY_FUNCTION__
#endif

#if defined(AM_BUILD_DLL)
    #if defined(_WIN32)
        #define AM_MODULE __declspec(dllexport)
    #else
        #define AM_MODULE
    #endif
#else
    #define AM_MODULE
#endif

#if defined(AM_ENABLE_AFTERMATH)
    AM_NORETURN extern void aftermath_finalize_crash() noexcept;

    #define AM_FINALIZE_VULKAN_CRASH() aftermath_finalize_crash()
#else
    #define AM_FINALIZE_VULKAN_CRASH() AM_ASSERT(false, "vulkan check failed");
#endif

#if defined(AM_DEBUG)
    #include <cassert>
    #define AM_ASSERT(expr, msg) assert((expr) && (msg))
    #define AM_VULKAN_CHECK(logger, expr)                                               \
        do {                                                                            \
            const auto __result = (expr);                                               \
            AM_UNLIKELY_IF(__result != VK_SUCCESS) {                                    \
                logger->critical("fatal vulkan failure: {}", prv::as_string(__result)); \
                AM_FINALIZE_VULKAN_CRASH();                                             \
            }                                                                           \
        } while (false)
#else
    #define AM_ASSERT(expr, msg) (void)(expr)
    #define AM_VULKAN_CHECK(logger, expr) ((void)(logger), AM_ASSERT(expr, 0))
#endif

#if AM_CPP_VERSION >= 202002l
    #define AM_LIKELY [[likely]]
    #define AM_UNLIKELY [[unlikely]]
#else
    #define AM_LIKELY
    #define AM_UNLIKELY
#endif

#if AM_CPP_VERSION >= 201703l
    #define AM_FALLTHROUGH [[fallthrough]]
#else
    #define AM_FALLTHROUGH
#endif

#if defined(__clang__) || defined(__GNUC__)
    #define AM_LIKELY_IF(cnd) if (__builtin_expect(!!(cnd), true))
    #define AM_UNLIKELY_IF(cnd) if (__builtin_expect(!!(cnd), false))
#elif defined(_MSC_VER) && AM_CPP_VERSION >= 202002l
    #define AM_LIKELY_IF(cnd) if (!!(cnd)) AM_LIKELY
    #define AM_UNLIKELY_IF(cnd) if (!!(cnd)) AM_UNLIKELY
#else
    #define AM_LIKELY_IF(cnd) if (cnd)
    #define AM_UNLIKELY_IF(cnd) if (cnd)
#endif

#if defined(AM_DEBUG_LOGGING)
    #define AM_LOG_DEBUG(logger, ...) (logger)->debug(__VA_ARGS__)
    #define AM_LOG_INFO(logger, ...) (logger)->info(__VA_ARGS__)
    #define AM_LOG_WARN(logger, ...) (logger)->warn(__VA_ARGS__)
    #define AM_LOG_ERROR(logger, ...) (logger)->error(__VA_ARGS__)
    #define AM_LOG_CRITICAL(logger, ...) (logger)->critical(__VA_ARGS__)
#else
    #define AM_LOG_DEBUG(logger, ...) (void)(logger)
    #define AM_LOG_INFO(logger, ...) (void)(logger)
    #define AM_LOG_WARN(logger, ...) (void)(logger)
    #define AM_LOG_ERROR(logger, ...) (void)(logger)
    #define AM_LOG_CRITICAL(logger, ...) (void)(logger)
#endif

#if defined(AM_ENABLE_PROFILING)
    #include <Tracy.hpp>
    #define AM_PROFILE_NAMED_SCOPE(name) ZoneScopedN(name) (void)0
    #define AM_PROFILE_SCOPED() ZoneScoped (void)0
    #define AM_MARK_FRAME() FrameMark (void)0
#else
    #define AM_PROFILE_NAMED_SCOPE(name) (void)(name)
    #define AM_PROFILE_SCOPED()
    #define AM_MARK_FRAME()
#endif

#define AM_MAKE_HASHABLE(T, name, ...)                               \
    template <>                                                      \
    struct hash<T> {                                                 \
        constexpr size_t operator ()(const T& name) const noexcept { \
            return am::prv::hash(0, __VA_ARGS__);                    \
        }                                                            \
    }
