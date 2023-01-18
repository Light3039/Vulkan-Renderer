#include "BindlessVk/Common/Aliases.hpp"

#include <exception>
#include <fmt/format.h>

namespace BINDLESSVK_NAMESPACE {

struct BindlessVkException: std::exception
{
	BindlessVkException(const str& what): msg(what)
	{
	}

	virtual c_str what() const noexcept
	{
		return msg.c_str();
	}

	str msg;
};

template<typename Expr, typename... Args>
inline void assert_true(const Expr& expr, fmt::format_string<Args...> fmt = "", Args&&... args)
{
	if (!static_cast<bool>(expr)) [[unlikely]]
	{
		throw BindlessVkException(fmt::format(fmt, std::forward<Args>(args)...));
	}
}

template<typename T, typename... Args>
inline void assert_false(const T& expr, fmt::format_string<Args...> fmt = "", Args&&... args)
{
	assert_true(!static_cast<bool>(expr), fmt, std::forward<Args>(args)...);
}

template<typename Expr1, typename Expr2, typename... Args>
inline void assert_eq(const Expr1& expr1, const Expr2& expr2, Args&&... args)
{
	assert_true(expr1 == expr2, std::forward<Args>(args)...);
}

template<typename Expr1, typename Expr2, typename... Args>
inline void assert_nq(const Expr1& expr1, const Expr2& expr2, Args&&... args)
{
	assert_true(expr1 != expr2, std::forward<Args>(args)...);
}

template<typename... Args>
inline void assert_fail(fmt::format_string<Args...> fmt, Args&&... args)
{
	throw BindlessVkException(fmt::format(fmt, std::forward<Args>(args)...));
}

} // namespace BINDLESSVK_NAMESPACE
