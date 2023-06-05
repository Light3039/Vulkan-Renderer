#pragma once

#include "Aliases.hpp"

#include <exception>
#include <format>
#include <string>
#include <type_traits>

/** The base exception class */
struct Exception: std::exception
{
	Exception(str const &what): msg(what)
	{
	}

	auto virtual what() const noexcept -> c_str
	{
		return msg.c_str();
	}

	str msg;
};

namespace details {

template<class F, class T, class = T>
struct is_static_castable: std::false_type
{
};

template<class F, class T>
struct is_static_castable<F, T, decltype(static_cast<T>(std::declval<F>()))>: std::true_type
{
};


template<typename Expr, typename... Args>
void inline throw_exception(Expr const &expr, str_view msg)
{
	auto what = str { msg };

	if constexpr (is_static_castable<Expr, int>())
		what = std::format("{} - expr({})", msg, int(expr));

	throw Exception(what);
}

} // namespace details

/** Throws bindlessvk exception
 *
 * @param fmt Format of the message (fmtlib syntax)
 * @param args Arguments of the message
 */
template<typename... Args>
void inline assert_fail(std::format_string<Args...> fmt = "", Args &&...args)
{
	details::throw_exception(0, std::format(fmt, std::forward<Args>(args)...));
}

/** Throws bindlessvk exception if @a expr evaluates to false
 *
 * @param expr The expression to check against
 * @param fmt Format of the message (fmtlib syntax)
 * @param args Arguments of the message
 */
template<typename Expr, typename... Args>
void inline assert_true(Expr const &expr, std::format_string<Args...> fmt = "", Args &&...args)
{
	if (!static_cast<bool>(expr))
		details::throw_exception(expr, std::format(fmt, std::forward<Args>(args)...));
}

/** Throws bindlessvk exception if @a expr evaluates to true
 *
 * @param expr The expression to check against
 * @param fmt Format of the message (fmtlib syntax)
 * @param args Arguments of the message
 */
template<typename Expr, typename... Args>
void inline assert_false(Expr const &expr, std::format_string<Args...> fmt = "", Args &&...args)
{
	if (static_cast<bool>(expr))
		details::throw_exception(expr, std::format(fmt, std::forward<Args>(args)...));
}
