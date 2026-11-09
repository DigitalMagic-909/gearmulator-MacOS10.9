// MAC-LEGACY (gearmulator fork): pieces of the C++ runtime that the system libc++ of older macOS versions lacks.
//
// std::bad_variant_access lives in libc++.dylib only from macOS 10.13: code that may throw it (std::get on a
// std::variant, std::visit) needs its vtable and typeinfo, and OS X 10.9 refuses to load a binary that imports
// them. what() is the class's key function, so defining it here makes the compiler emit both into this binary,
// where the linker binds every reference to them. The availability annotations, which mark the class unavailable
// below 10.13, are switched off for this file only.
// Linked only for macOS deployment targets below 10.13 (see the top-level CMakeLists.txt).

#define _LIBCPP_DISABLE_AVAILABILITY 1
#define _LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS 1

#include <variant>

// libc++ declares it directly in namespace std, outside its versioned __1 namespace (the imported symbols are
// __ZTISt18bad_variant_access / __ZTVSt18bad_variant_access)
namespace std
{
	const char* bad_variant_access::what() const noexcept
	{
		return "bad_variant_access";
	}
}
