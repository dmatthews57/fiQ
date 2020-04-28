#pragma once
//==========================================================================================================================
// SteadyClock.h : Classes and functions for tracking event duration and scheduling
//==========================================================================================================================

#include <chrono>

namespace FIQCPPBASE {

//==========================================================================================================================
// SteadyClock: Class wrapping a chrono::steady_clock value with millisecond resolution, for non-wall-clock durations
// - Note that duration_cast calls in this class are casting values to millisecond resolution, not number of milliseconds
class SteadyClock {
public:
	//======================================================================================================================
	// Public named constructors
	static SteadyClock Now() noexcept {return SteadyClock();}
	static SteadyClock NowPlus(std::chrono::steady_clock::duration d) {
		SteadyClock s;
		s.t += std::chrono::duration_cast<std::chrono::milliseconds>(d);
		return s;
	}

	//======================================================================================================================
	// Comparison operators
	_Check_return_ bool IsPast() const noexcept {
		return (std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()) >= t);
	}
	_Check_return_ bool operator > (const SteadyClock& sc) const noexcept {return (t > sc.t);}
	_Check_return_ bool operator >= (const SteadyClock& sc) const noexcept {return (t >= sc.t);}
	_Check_return_ bool operator < (const SteadyClock& sc) const noexcept {return (t < sc.t);}
	_Check_return_ bool operator <= (const SteadyClock& sc) const noexcept {return (t <= sc.t);}
	_Check_return_ bool operator == (const SteadyClock& sc) const noexcept {return (t == sc.t);}

	//======================================================================================================================
	// Aritmetic operators
	friend SteadyClock operator + (const SteadyClock& base, std::chrono::steady_clock::duration d) {
		SteadyClock sc(base);
		sc.t += std::chrono::duration_cast<std::chrono::milliseconds>(d);
		return sc;
	}
	SteadyClock& operator += (std::chrono::steady_clock::duration d) {
		t += std::chrono::duration_cast<std::chrono::milliseconds>(d);
		return *this;
	}
	friend SteadyClock operator - (const SteadyClock& base, std::chrono::steady_clock::duration d) {
		SteadyClock sc(base);
		sc.t -= std::chrono::duration_cast<std::chrono::milliseconds>(d);
		return sc;
	}
	SteadyClock& operator -= (std::chrono::steady_clock::duration d) {
		t -= std::chrono::duration_cast<std::chrono::milliseconds>(d);
		return *this;
	}

	//======================================================================================================================
	// Difference operators - return duration from input time point to this time point, forwards or backwards
	template<typename T>
	_Check_return_ auto Since(const SteadyClock& s) const noexcept {
		return std::chrono::duration_cast<T>(t - s.t).count();
	}
	template<typename T>
	_Check_return_ auto Till(const SteadyClock& s) const noexcept {
		return std::chrono::duration_cast<T>(s.t - t).count();
	}
	// Millisecond specializations: Most commonly used, provided to make clients less verbose:
	_Check_return_ int MSecSince(const SteadyClock& s) const noexcept {
		return gsl::narrow_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t - s.t).count());
	}
	_Check_return_ int MSecTill(const SteadyClock& s) const noexcept {
		return gsl::narrow_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(s.t - t).count());
	}

	//======================================================================================================================
	// Timestamp update accessors
	SteadyClock& SetNow() noexcept {
		t = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
		return *this;
	}
	void SetNowPlus(std::chrono::steady_clock::duration d) {
		t = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
		t += std::chrono::duration_cast<std::chrono::milliseconds>(d);
	}

	//======================================================================================================================
	// Defaulted public constructors/assignment operators/destructor
	SteadyClock() noexcept = default;
	SteadyClock(const SteadyClock&) = default;
	SteadyClock(SteadyClock&&) = default;
	SteadyClock& operator=(const SteadyClock&) = default;
	SteadyClock& operator=(SteadyClock&&) = default;
	~SteadyClock() noexcept = default;
	// Custom public constructor - current time offset by a duration
	explicit SteadyClock(std::chrono::steady_clock::duration d)
		: t(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now())) {
		t += std::chrono::duration_cast<std::chrono::milliseconds>(d);
	}
	// Custom public constructor - copy from another object, offset by a duration
	SteadyClock(const SteadyClock& sc, std::chrono::steady_clock::duration d) : t(sc.t) {
		t += std::chrono::duration_cast<std::chrono::milliseconds>(d);
	}

private:
	using SteadyMSPoint = std::chrono::time_point<std::chrono::steady_clock,std::chrono::milliseconds>;
	SteadyMSPoint t = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
};

}; // (end namespace FIQCPPBASE)
