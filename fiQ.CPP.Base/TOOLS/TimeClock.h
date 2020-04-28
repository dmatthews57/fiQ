#pragma once
//==========================================================================================================================
// TimeClock.h : Classes and functions for dealing with wall-clock time values
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
	// Const accessors to time variables
	_Check_return_ const time_t GetSeconds() const noexcept {return Timestamp.time;}
	_Check_return_ const unsigned short GetMilliseconds() const noexcept {return Timestamp.millitm;}
	_Check_return_ const tm& GetLocalTime() const {
		if(DirtyTime) {
			localtime_s(&LocalTime, &Timestamp.time);
			DirtyTime = false;
		}
		return LocalTime;
	}
	//======================================================================================================================
	// Timestamp update accessors
	void SetNow() {
		_ftime_s(&Timestamp);
		DirtyTime = true;
	}

	//======================================================================================================================
	// Serialization functions
	_Check_return_ bool SerializeTo(const SerialOps::Stream& sh) const {return sh.Write(Timestamp);}
	_Check_return_ bool ReadFrom(const SerialOps::Stream& sh) {return sh.Read(Timestamp);}

	//======================================================================================================================
	// Public constructors
	TimeClock() noexcept(false) {_ftime_s(&Timestamp);}
	explicit TimeClock(const _timeb& TimeBase) noexcept : Timestamp(TimeBase) {}
	// Public named constructor (if preferred for readability)
	static TimeClock Now() {return TimeClock();}

private:
	_timeb Timestamp = {0};			// Container for wall time (seconds and milliseconds)
	mutable bool DirtyTime = true;	// Flag indicating whether LocalTime has been set since Timestamp updated)
	mutable tm LocalTime = {0};		// tm structure, populated on demand by accessor
};

}; // (end namespace FIQCPPBASE)
