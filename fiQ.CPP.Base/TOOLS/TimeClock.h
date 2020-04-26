#pragma once
//==========================================================================================================================
// TimeClock.h : Defines classes for dealing with timestamps and durations
//==========================================================================================================================

#include <chrono>
#include <sys/timeb.h>
#include "Tools/SerialOps.h"

namespace FIQCPPBASE {

//==========================================================================================================================
// TimeClock: Class wrapping _timeb structure to provide rollover-safe wall-clock timestamp with milliseconds
class TimeClock
{
public:
	//======================================================================================================================
	// Comparison operators
	_Check_return_ bool Expired() const {
		_timeb TNow = {0}; _ftime_s(&TNow);
		return (TNow.time > Timestamp.time ? true
			: (TNow.time == Timestamp.time && TNow.millitm >= Timestamp.millitm));
	}
	_Check_return_ bool operator > (const TimeClock& tc) const noexcept {
		return (Timestamp.time > tc.Timestamp.time ? true
			: (Timestamp.time == tc.Timestamp.time && Timestamp.millitm > tc.Timestamp.millitm));
	}
	_Check_return_ bool operator >= (const TimeClock& tc) const noexcept {
		return (Timestamp.time > tc.Timestamp.time ? true
			: (Timestamp.time == tc.Timestamp.time && Timestamp.millitm >= tc.Timestamp.millitm));
	}
	_Check_return_ bool operator < (const TimeClock& tc) const noexcept {
		return (Timestamp.time < tc.Timestamp.time ? true
			: (Timestamp.time == tc.Timestamp.time && Timestamp.millitm < tc.Timestamp.millitm));
	}
	_Check_return_ bool operator <= (const TimeClock& tc) const noexcept {
		return (Timestamp.time < tc.Timestamp.time ? true
			: (Timestamp.time == tc.Timestamp.time && Timestamp.millitm <= tc.Timestamp.millitm));
	}
	_Check_return_ bool operator == (const TimeClock& tc) const noexcept {
		return (Timestamp.time == tc.Timestamp.time && Timestamp.millitm == tc.Timestamp.millitm);
	}

	//======================================================================================================================
	// Difference operators - return time from input timestamp to this timestamp, in seconds or milliseconds
	_Check_return_ int MSecSince(const TimeClock& tc) const noexcept {
		// Note: Beyond max size of int, return value undefined (assumes timestamps are a reasonable distance apart)
		return (gsl::narrow_cast<int>((Timestamp.time - tc.Timestamp.time) * 1000) + (Timestamp.millitm - tc.Timestamp.millitm));
	}
	_Check_return_ time_t SecSince(const TimeClock& tc) const noexcept {
		// Note: Adds 5 milliseconds before rounding to account for clock sampling error
		return (((((Timestamp.time - tc.Timestamp.time) * 1000) + (Timestamp.millitm - tc.Timestamp.millitm)) + 5) / 1000);
	}
	// Return time from now until input timestamp (same operation, different readability)
	_Check_return_ int MSecTill(const TimeClock& tc) const noexcept {
		return tc.MSecSince(*this);
	}
	_Check_return_ time_t SecTill(const TimeClock& tc) const noexcept {
		return tc.SecSince(*this);
	}
	//======================================================================================================================
	// Time variable const accessors
	_Check_return_ const time_t& GetSeconds() const noexcept {return Timestamp.time;}
	_Check_return_ const unsigned short GetMilliseconds() const noexcept {return Timestamp.millitm;}
	//======================================================================================================================
	// Serialization functions
	_Check_return_ bool SerializeTo(const SerialOps::Stream& sh) const {return sh.Write(Timestamp);}
	_Check_return_ bool ReadFrom(const SerialOps::Stream& sh) {return sh.Read(Timestamp);}
	//======================================================================================================================
	// Timestamp update accessors
	void AddMSec(unsigned int ms) noexcept {
		// Add full seconds to time, then milliseconds to millitm; if this rolls millitm over 1000, add adjusted seconds
		// to time (this is done in two steps to allow safe conversion between time_t and unsigned short)
		Timestamp.time += (ms / 1000);
		Timestamp.millitm += gsl::narrow_cast<unsigned short>(ms % 1000U);
		if(Timestamp.millitm >= 1000) {
			Timestamp.time += (Timestamp.millitm / 1000);
			Timestamp.millitm %= 1000;
		}
	}
	void SetNow() {_ftime_s(&Timestamp);}
	void SetNowPlusMSec(unsigned int ms) {_ftime_s(&Timestamp); AddMSec(ms);}
	//======================================================================================================================
	// Public constructors
	TimeClock(const TimeClock& tc, unsigned int PlusMS) noexcept : Timestamp(tc.Timestamp) {AddMSec(PlusMS);}
	explicit TimeClock(unsigned int PlusMS) {_ftime_s(&Timestamp); AddMSec(PlusMS);}
	explicit TimeClock(const _timeb& TimeBase) noexcept : Timestamp(TimeBase) {}
	// Public named constructors
	static TimeClock Now() {
		TimeClock now;
		now.SetNow();
		return now;
	}

private:
	TimeClock() noexcept = default;
	_timeb Timestamp = {0};
};

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

} // (end namespace FIQCPPBASE)
