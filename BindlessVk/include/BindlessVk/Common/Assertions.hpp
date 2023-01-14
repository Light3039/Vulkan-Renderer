#include "BindlessVk/Common/Aliases.hpp"

#include <exception>
#include <source_location>

namespace BINDLESSVK_NAMESPACE {

struct BindlessVkException: std::exception
{
	BindlessVkException(
	    const str& what,
	    const std::source_location location = std::source_location::current()
	)
	    : msg(what.c_str())
	    , location(location)
	{
	}

	virtual c_str what() const noexcept
	{
		return msg;
	}

	c_str msg;
	c_str expr;
	std::source_location location;
};

template<typename T>
inline void assert_true(
    const T& expr,
    const str& message = "",
    const std::source_location location = std::source_location::current()
)
{
	if (!static_cast<bool>(expr)) [[unlikely]]
	{
		throw BindlessVkException(message, location);
	}
}

template<typename T>
inline void assert_false(
    const T& expr,
    const str& message = "",
    const std::source_location location = std::source_location::current()
)
{
	assert_true(!static_cast<bool>(expr), message, location);
}

template<typename T1, typename T2>
inline void assert_eq(
    const T1& rhs,
    const T2& lhs,
    const str& message = "",
    const std::source_location location = std::source_location::current()
)
{
	assert_true(rhs == lhs, message, location);
}

template<typename T1, typename T2>
inline void assert_nq(
    const T1& rhs,
    const T1& lhs,
    const str& message = "",
    const std::source_location location = std::source_location::current()
)
{
	assert_true(rhs != lhs, message, location);
}

inline void assert_fail(
    const str& message = "",
    const std::source_location location = std::source_location::current()
)
{
	throw BindlessVkException(message, location);
}

} // namespace BINDLESSVK_NAMESPACE
