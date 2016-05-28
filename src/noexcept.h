#if defined _MSC_VER
#if _MSC_VER >= 1900
#define NOEXCEPT noexcept
#else
#define NOEXCEPT
#endif
#else
#define NOEXCEPT noexcept
#endif
