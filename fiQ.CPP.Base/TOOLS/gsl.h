#pragma once
//==========================================================================================================================
// gsl.h : Useful utilities pulled from Microsoft's Guidelines Support Library
//==========================================================================================================================
// Actual library is full of code analysis problems - this file is just a subset of GSL functionality, and is kept in same
// namespace with same naming convention in case the official library (or another version) can be integrated
//==========================================================================================================================
namespace gsl {

#define GSL_SUPPRESS(x) [[gsl::suppress(x)]]

// narrow_cast(): a searchable way to do narrowing casts of values
template <class T, class U>
GSL_SUPPRESS(type.1)
constexpr T narrow_cast(U&& u) noexcept
{
	return static_cast<T>(std::forward<U>(u));
}

} // (end namespace gsl)