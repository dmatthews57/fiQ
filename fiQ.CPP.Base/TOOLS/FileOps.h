#pragma once
//==========================================================================================================================
// FileOps.h : Classes and functions for file I/O management
//==========================================================================================================================

namespace FIQCPPBASE {

class FileOps
{
public:

	//======================================================================================================================
	// Public type definitions
	using FilePtr = std::unique_ptr<FILE, decltype(&fclose)>;

	//======================================================================================================================
	// Open function for unique_ptr to file
	static FilePtr OpenFile(_In_z_ const char* FileName, _In_z_ const char* Mode, int ShFlag = _SH_DENYRD) {
		return FilePtr(_fsopen(FileName, Mode, ShFlag), fclose);
	}

};

} // (end namespace FIQCPPBASE)
