#pragma once
//==========================================================================================================================
// ValueOps.h : Classes and functions for performing simple value-based operations
//==========================================================================================================================

namespace FIQCPPBASE {

class ValueOps
{
public:

	//======================================================================================================================
	// IsImpl: Container class for value-checking functions
	// - Create using "Is" function below to take advantage of template deduction, e.g. "Is(a).InSet(1,2,3,4)"
	// - This object will typically be optimized out by the compiler as a result
	template<typename T>
	class IsImpl {
	public:

		// InSet: Check whether value is equal to any of a set of input values
		template<typename...Args>
		_Check_return_ constexpr bool InSet(const T& first, const Args&... args) const noexcept {
			static_assert(all_same_v<T, Args...>,
				"All types in InSet parameter pack must be the same");
			return (t == first || this->InSet(args...));
		}

		// InRange: Check whether value is within the specified range (inclusive of "left" and "right" values)
		_Check_return_ constexpr bool InRange(const T& left, const T& right) const noexcept {
			return (t >= left && t <= right);
		}
		// InRangeLeft: Check whether value is within the specified range (inclusive of "left" value but excluding "right")
		_Check_return_ constexpr bool InRangeLeft(const T& left, const T& right) const noexcept {
			return (t >= left && t < right);
		}
		// InRangeRight: Check whether value is within the specified range (exclusive of "left" value but including "right")
		_Check_return_ constexpr bool InRangeRight(const T& left, const T& right) const noexcept {
			return (t > left && t <= right);
		}
		// InRangeEx: Check whether value is within the specified range (exclusive of "left" and "right" values)
		_Check_return_ constexpr bool InRangeEx(const T& left, const T& right) const noexcept {
			return (t > left && t < right);
		}
		// InRangeSet: Check whether value is within any of the specified ranges (inclusive of "left" and "right" values)
		// - Use std::make_pair to provide std::pair<T,T> values to pass to this function
		template<typename...Args>
		_Check_return_ constexpr bool InRangeSet(const std::pair<T,T>&& p, Args... args) const noexcept {
			static_assert(all_same_v<std::pair<T,T>, Args...>,
				"All types in InRangeSet parameter pack must be the same");
			return (t >= p.first && t <= p.second) ? true : this->InRangeSet(std::forward<Args>(args)...);
		}

		constexpr IsImpl(const T& _t) noexcept : t(_t) {}

	private:
		// Private end-state functions for variadic template functions above
		_Check_return_ constexpr bool InSet(const T& last) const noexcept {
			return (t == last);
		}
		_Check_return_ constexpr bool InRangeSet(const std::pair<T,T>&& p) const noexcept {
			return (t >= p.first && t <= p.second);
		}
		// Private reference to input value
		const T& t;
	};
	// Is: Helper function to deduce template argument and instantiate IsImpl object
	template<typename T>
	_Check_return_ static constexpr IsImpl<T> Is(const T& t) noexcept {return IsImpl<T>(t);}

	//======================================================================================================================
	// Common math functions - Compile-time exponent calculation for integral values
	template<typename T, T base, unsigned char Exponent>
	struct CalcExponent {
		static constexpr T value = (base * CalcExponent<T, base, Exponent - 1>::value);
	};
	template<typename T, T base>
	struct CalcExponent<T, base, 0> { // Partial specialization for terminal case (exponent zero)
		static constexpr T value = 1;
	};
	// Common math functions - powers of 10
	_Check_return_ static constexpr unsigned long long PowerOf10(size_t Exponent) noexcept {
		return Exponent < _countof(PowersOf10) ? PowersOf10[Exponent] : 0;
	}
	// Common math functions - Integral value with minimum zero (i.e. treat negatives as zero)
	template<typename T, std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, int> = 0>
	_Check_return_ static constexpr T MinZero(T t) noexcept {return (t > 0 ? t : 0);}
	// Common math functions - Return value within specified upper and lower boundary:
	template<typename T>
	_Check_return_ static constexpr T Bounded(T lower, T value, T upper) noexcept {
		return (value < lower ? lower : (value > upper ? upper : value));
	}

private:

	// Type-checking helpers - Ensure that all members of a parameter pack are of the same type:
	template<typename T, typename...>
	struct all_same : std::true_type {};
	template<typename T1, typename T2, typename...Args>
	struct all_same<T1, T2, Args...>
		: std::integral_constant<bool, std::is_same_v<std::decay_t<T1>, std::decay_t<T2> > && all_same<T1, Args...>::value > {};
	// Value alias
	template<typename...Args>
	static constexpr auto all_same_v = all_same<Args...>::value;

	// Internal constant expressions:
	static constexpr unsigned long long PowersOf10[20] = {
		CalcExponent<unsigned long long, 10, 0>::value,
		CalcExponent<unsigned long long, 10, 1>::value,
		CalcExponent<unsigned long long, 10, 2>::value,
		CalcExponent<unsigned long long, 10, 3>::value,
		CalcExponent<unsigned long long, 10, 4>::value,
		CalcExponent<unsigned long long, 10, 5>::value,
		CalcExponent<unsigned long long, 10, 6>::value,
		CalcExponent<unsigned long long, 10, 7>::value,
		CalcExponent<unsigned long long, 10, 8>::value,
		CalcExponent<unsigned long long, 10, 9>::value,
		CalcExponent<unsigned long long, 10, 10>::value,
		CalcExponent<unsigned long long, 10, 11>::value,
		CalcExponent<unsigned long long, 10, 12>::value,
		CalcExponent<unsigned long long, 10, 13>::value,
		CalcExponent<unsigned long long, 10, 14>::value,
		CalcExponent<unsigned long long, 10, 15>::value,
		CalcExponent<unsigned long long, 10, 16>::value,
		CalcExponent<unsigned long long, 10, 17>::value,
		CalcExponent<unsigned long long, 10, 18>::value,
		CalcExponent<unsigned long long, 10, 19>::value
	};
};

}; // (end namespace FIQCPPBASE)
