#pragma once
//==========================================================================================================================
// TimeClock.h : Define class wrapping _timeb structure to provide rollover-safe timestamp with milliseconds
//==========================================================================================================================

#include <sys/timeb.h>
#include "SerialOps.h"

namespace FIQCPPBASE {

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

} // (end namespace FIQCPPBASE)
