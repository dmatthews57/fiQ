#pragma once
//==========================================================================================================================
// SerialOps.h : Classes and functions for assisting with serialization of objects
//==========================================================================================================================

#include <sys/timeb.h>
#include "ValueOps.h"

namespace FIQCPPBASE {

class SerialOps
{
public:

	//======================================================================================================================
	// Serialization stream base class definition
	class Stream {
	public:
		//==================================================================================================================
		// Strongly-typed serialization implemented by base class:
		template<typename T>
		_Check_return_ bool Read(T& Tgt) const;
		template<typename T>
		_Check_return_ bool Write(const T& Src) const;
		// String serialization implemented by base class:
		_Check_return_ bool Read(_Out_writes_(TgtSize) char* Tgt, size_t TgtSize) const;
		_Check_return_ bool Write(_In_reads_(SrcSize) const char* Src, size_t SrcSize) const;

		Stream() noexcept = default;
		Stream(const Stream&) = delete;
		Stream(Stream&&) = delete;
		Stream& operator=(const Stream&) = delete;
		Stream& operator=(Stream&&) = delete;
		virtual ~Stream() = default;

	protected:
		// Untyped (pointer/size-based) functions must be implemented by child class:
		_Check_return_ virtual bool DoRead(_Out_writes_(TgtSize) void* Tgt, size_t TgtSize) const = 0;
		_Check_return_ virtual bool DoWrite(_In_reads_(SrcSize) const void* Src, size_t SrcSize) const = 0;
	};

	//======================================================================================================================
	// File-based serialization stream class
	class FileStream : public Stream {
	public:
		FileStream(_In_ FILE* _FHandle) noexcept : FHandle(_FHandle) {}
	private:
		// Base class function implementations
		_Check_return_ bool DoRead(_Out_writes_(TgtSize) void* Tgt, size_t TgtSize) const override;
		_Check_return_ bool DoWrite(_In_reads_(SrcSize) const void* Src, size_t SrcSize) const override;
		// Private member variables
		FILE* const FHandle;
	};
	class MemoryStream : public Stream {
	public:
		MemoryStream(_Inout_updates_(_MemBufSize) char* _MemBuf, size_t _MemBufSize) noexcept
			: MemBuf(_MemBuf), MemBufSize(_MemBufSize) {}
	private:
		// Base class function implementations
		_Check_return_ bool DoRead(_Out_writes_(TgtSize) void* Tgt, size_t TgtSize) const override;
		_Check_return_ bool DoWrite(_In_reads_(SrcSize) const void* Src, size_t SrcSize) const override;
		// Private member variables
		mutable char* MemBuf;		// Members are mutable to allow const member functions to modify
		mutable size_t MemBufSize;	// (since this object will only be accessible through const handles)
	};

private:

	//======================================================================================================================
	// Type-checking helpers - ensure only approved types are used with strongly-typed functions
	template<typename T, typename = void>
	struct valid_type : std::false_type {};	// Catch-all definition, default to false
	// Partial specialization for enum values - allow if underlying type is arithmetic
	template<typename T>
	struct valid_type<T, typename std::enable_if_t<std::is_enum_v<T>>>
		: std::is_arithmetic<typename std::underlying_type_t<T>> {};
	// Partial specialization for arithmetic values (all allowed)
	template<typename T>
	struct valid_type<T, typename std::enable_if_t<std::is_arithmetic_v<T>>> : std::true_type {};
	// Partial specializations for specific allowed non-arithmetic types
	template<>
	struct valid_type<_timeb> : std::true_type {};
	template<>
	struct valid_type<tm> : std::true_type {};
	// Value alias
	template<typename T>
	static constexpr auto valid_type_v = valid_type<T>::value;

};

//==========================================================================================================================
// Stream function specializations - Validate types and pass to child virtual functions
template<>
GSL_SUPPRESS(r.10) // Want to use _malloca here, which implicitly allows malloc
_Check_return_ inline bool SerialOps::Stream::Read<std::string>(std::string& Tgt) const {
	// Read and validate incoming length (note upper boundary may need to be expanded in the future if required): 
	uint32_t len = 0;
	if(this->Read(len)) {
		if(ValueOps::Is(len).InRange(0, (std::numeric_limits<decltype(len)>::max)())) {
			// Allocate temporary buffer for string, read and assign to target
			std::unique_ptr<char, decltype(&_freea)> TempBuf(static_cast<char*>(_malloca(static_cast<size_t>(len) + 10)), _freea);
			if(TempBuf.get()) {
				if(this->DoRead(TempBuf.get(), len)) {
					Tgt.assign(TempBuf.get(), len);
					TempBuf.reset(nullptr);
					return true;
				}
			}
		}
	}
	return false;
}
template<>
_Check_return_ inline bool SerialOps::Stream::Write<std::string>(const std::string& Src) const {
	const uint32_t len = gsl::narrow_cast<uint32_t>(ValueOps::Bounded<size_t>(0, Src.length(), (std::numeric_limits<decltype(len)>::max)()));
	// Write 32-bit length, if successful follow with actual data
	if(this->Write(len)) return this->Write(Src.data(), len);
	else return false;
}
// Stream function implementations - Validate primitive types and pass to child virtual functions
template<typename T>
_Check_return_ inline bool SerialOps::Stream::Read(T& Tgt) const {
	static_assert(SerialOps::valid_type_v<T>, "Invalid type for template serialization");
	return this->DoRead(&Tgt, sizeof(T));
}
template<typename T>
_Check_return_ inline bool SerialOps::Stream::Write(const T& Src) const {
	static_assert(SerialOps::valid_type_v<T>, "Invalid type for template serialization");
	return this->DoWrite(&Src, sizeof(T));
}
// Stream function implementations - Pass strings to child virtual functions
_Check_return_ inline bool SerialOps::Stream::Read(_Out_writes_(TgtSize) char* Tgt, size_t TgtSize) const {
	return this->DoRead(Tgt, TgtSize);
}
_Check_return_ inline bool SerialOps::Stream::Write(_In_reads_(SrcSize) const char* Src, size_t SrcSize) const {
	return this->DoWrite(Src, SrcSize);
}

//==========================================================================================================================
// FileStream function implementations
_Check_return_ inline bool SerialOps::FileStream::DoRead(_Out_writes_(TgtSize) void* Tgt, size_t TgtSize) const {
	try {return (FHandle ? (fread(Tgt, TgtSize, 1, FHandle) == 1) : false);}
	catch(const std::exception&) {return false;}
}
_Check_return_ inline bool SerialOps::FileStream::DoWrite(_In_reads_(SrcSize) const void* Src, size_t SrcSize) const {
	try {return (FHandle ? (fwrite(Src, SrcSize, 1, FHandle) == 1) : false);}
	catch(const std::exception&) {return false;}
}

//==========================================================================================================================
// MemoryStream function implementations
_Check_return_ inline bool SerialOps::MemoryStream::DoRead(_Out_writes_(TgtSize) void* Tgt, size_t TgtSize) const {
	try {
		if(MemBuf && MemBufSize >= TgtSize) {
			// Read data from member buffer to target, advance pointer and deduct from size counter
			memcpy(Tgt, MemBuf, TgtSize);
			MemBuf += TgtSize;
			MemBufSize -= TgtSize;
			return true;
		}
	}
	catch(const std::exception&) {} // No action required, just return false
	return false;
}
_Check_return_ inline bool SerialOps::MemoryStream::DoWrite(_In_reads_(SrcSize) const void* Src, size_t SrcSize) const {
	try {
		if(MemBuf && MemBufSize >= SrcSize) {
			// Write data to member buffer, advance pointer and deduct from size counter
			memcpy(MemBuf, Src, SrcSize);
			MemBuf += SrcSize;
			MemBufSize -= SrcSize;
			return true;
		}
	}
	catch(const std::exception&) {} // No action required, just return false
	return false;
}

} // (end namespace FIQCPPBASE)
