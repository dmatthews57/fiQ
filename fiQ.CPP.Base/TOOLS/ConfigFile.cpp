//==========================================================================================================================
// ConfigFile.cpp : Classes for loading and accessing configuration files
//==========================================================================================================================
#include "pch.h"
#include "ConfigFile.h"
#include "Exceptions.h"
#include "FileOps.h"
using namespace FIQCPPBASE;

//==========================================================================================================================
// ConfigSection::EmptyStr: Initialize and return empty static string
_Check_return_ const std::string& ConfigFile::ConfigSection::EmptyStr() {
	static const std::string EMPTYSTR;
	return EMPTYSTR;
}
// ConfigSection::ReadSection: Read lines from input file into ConfigEntry collection
template<size_t ReadBufSize>
_Check_return_ bool ConfigFile::ConfigSection::ReadSection(
	ConfigFile::pass_key, _Inout_ FILE* file, _Out_writes_(ReadBufSize) char* ReadBuf) {
	// Ensure this function is being compiled with a valid read buffer size (must be compatible with int for fopen):
	static_assert(ReadBufSize < INT_MAX, "Invalid read buffer length");

	// If invalid file pointer received, return immediately
	if(file == nullptr) return false;

	const char * const EndReadBuf = ReadBuf + ReadBufSize;
	try {
		// Store starting position of file pointer, and loop until end of file or start of new section
		int filepos = ftell(file);
		while(fgets(ReadBuf, gsl::narrow_cast<int>(ReadBufSize), file)) {

			// Locate first non-whitespace character in line
			char *StartPtr = ReadBuf;
			for(StartPtr; StartPtr < EndReadBuf ? std::isspace(static_cast<unsigned char>(StartPtr[0])) : false; ++StartPtr);
			// Ensure StartPtr has not exceeded bound (shouldn't be possible, since null terminator should stop loop):
			if(StartPtr >= EndReadBuf) throw FORMAT_RUNTIME_ERROR("Unterminated line read from file (1)");
			// If this is the start of a new section, reset file pointer to start of this line and break immediately;
			// otherwise update stored file position to end of this line:
			else if(StartPtr[0] == '[') {
				fseek(file, filepos, SEEK_SET);
				break;
			}
			else filepos = ftell(file);

			// If line is zero length or commented out, skip to next line
			if(ValueOps::Is(StartPtr[0]).InSet(0, ';')) continue;

			// Iterate through rest of line, looking for (right-trimmed) end of data:
			char *ptr = StartPtr, *EndPtr = StartPtr, *Equals = nullptr;
			for(bool inQuote = false, inComment = false; ptr < EndReadBuf ? ptr[0] != 0 : false; ++ptr) {
				if(ptr[0] == '/') {
					if(ValueOps::Is(ptr[1]).InSet('/','*')) {
						// We are at opening of comment mark - if we are currently in a quoted section set flag
						// that we are now in comment (in case quoted section never closes), otherwise break
						if(inQuote) inComment = true;
						else break;
					}
				}
				else if(ptr[0] == '\"') {
					// Flip state of quoted section; if we are closing a quote, ensure we now ignore any comment
					// marks detected before this point
					if((inQuote = !inQuote) == false) inComment = false;
				}
				// If encountering first equals sign of this string and we are not inside a quote, store location
				else if(ptr[0] == '=' && Equals == nullptr && inQuote == false) Equals = ptr;

				// If this is a non-whitespace character (including slash, quote or equals) AND we are not currently
				// reading a comment, advance EndPtr to ensure bytes to this point are included in data
				if(!std::isspace(static_cast<unsigned char>(ptr[0])) && !inComment) EndPtr = ptr;
			}
			// Ensure ptr has not exceeded bound (shouldn't be possible, since null terminator should stop loop):
			if(ptr >= EndReadBuf) throw FORMAT_RUNTIME_ERROR("Unterminated line read from file (2)");
			// If non-zero-length data found, add this line to deque (advancing EndPtr ahead of end of data):
			else if(EndPtr > StartPtr) ConfigEntries.emplace_back(pass_key{}, StartPtr, ++EndPtr, Equals);
		}
		// If this point is reached, file was successfully read; return success if any entries added to collection:
		return (ConfigEntries.empty() == false);
	}
	catch(const std::exception&) {
		std::throw_with_nested(FORMAT_RUNTIME_ERROR("Exception reading configuration section"));
	}
}

//==========================================================================================================================
// ConfigFile::Initialize: Open specified file and read into ConfigSection collection
_Check_return_ bool ConfigFile::Initialize(_In_z_ const char* FileName) {
	try {
		// Attempt to open source file; if file cannot be opened, return failure immediately
		FileOps::FilePtr fp = FileOps::OpenFile(FileName, "r", _SH_DENYWR);
		if(fp.get() == nullptr) return false;

		// Set up read buffer (shared throughout this function)
		static constexpr size_t ReadBufSize = 1024;
		char ReadBuf[ReadBufSize + 5] = {0}, * const EndReadBuf = ReadBuf + ReadBufSize;

		// Read all lines from file, looking for [SECTIONHEADERS]
		while(fgets(ReadBuf, ReadBufSize, fp.get())) {
			// Locate first non-whitespace character in line; for some reason StartPtr has to be initialized at declaration
			// AND in body of loop, to satisfy code analyzer (it believes StartPtr is uninitialized otherwise):
			const char *StartPtr = ReadBuf;
			for(StartPtr = ReadBuf; StartPtr < EndReadBuf ? std::isspace(static_cast<unsigned char>(StartPtr[0])) : false; ++StartPtr);
			// Ensure StartPtr has not exceeded bound (shouldn't be possible, since null terminator should stop loop):
			if(StartPtr >= EndReadBuf) throw FORMAT_RUNTIME_ERROR("Unterminated line read from file (3)");
			// If first character is not opening of section name tag, ignore line and continue reading:
			else if(StartPtr[0] != '[') continue;

			// Advance to first non-whitespace character inside [] block; if not found or block is empty, discard (first
			// ensure StartPtr has not exceeded bound - shouldn't be possible, since null terminator should stop loop):
			for(++StartPtr; StartPtr < EndReadBuf ? std::isspace(static_cast<unsigned char>(StartPtr[0])) : false; ++StartPtr);
			if(StartPtr >= EndReadBuf) throw FORMAT_RUNTIME_ERROR("Unterminated line read from file (4)");
			else if(ValueOps::Is(StartPtr[0]).InSet(0, ']')) continue;

			// Locate closing ']' tag - use special logic if section name starts with quotation mark (allow for
			// a section name entirely enclosed in quotes), otherwise just perform straightforward search
			const char *EndPtr = nullptr;
			if(StartPtr[0] == '\"') {
				bool inQuotes = false;
				const char *LastQuote = nullptr;
				for(const char *ptr = StartPtr; (ptr < EndReadBuf ? ptr[0] != 0 : false); ++ptr) {
					if(ptr[0] == '\"') {
						// Flip current state of quotation text; if this is a closing quote, store location (if
						// it is an opening quote, ensure location of last closing quote is cleared):
						if((inQuotes = !inQuotes) == false) LastQuote = ptr;
						else LastQuote = nullptr;
					}
					else if(ptr[0] == ']') {
						// Closing tag found - if we are not currently inside quoted section, save current location
						// (or location of closing quote, if any) and break loop:
						if(inQuotes == false) {
							EndPtr = LastQuote ? LastQuote : ptr;
							++StartPtr; // Advance past opening quote
							break;
						}
						// Otherwise save current pointer location (and clear position of last quote, since there is
						// data beyond it), but do not break loop (as we prefer a closing section mark outside a quoted
						// area); note this could result in strange section headings including single quotes and multiple
						// ']' symbols, but we are doing our best to help user with naming/formatting flexibility
						EndPtr = ptr;
						LastQuote = nullptr;
					}
					// If data appears after last quotation mark, clear it so it is not used as end position
					else if(!std::isspace(static_cast<unsigned char>(ptr[0]))) LastQuote = nullptr;
				}
			}
			else EndPtr = strchr(StartPtr, ']');

			// If EndPtr is set to a valid position, this is a validated section-starting line:
			if(ValueOps::Is(EndPtr).InRangeEx(StartPtr, EndReadBuf)) {
				// Create new ConfigSection object, and hand off file reading to it (it will return when encountering end
				// of file or start of next section); if read successful, add object to member deque:
				std::shared_ptr<ConfigSection> cs = std::make_shared<ConfigSection>(
					pass_key{}, StringOps::Trim(StartPtr, EndPtr - StartPtr));
				if(cs->ReadSection<ReadBufSize>(pass_key{}, fp.get(), ReadBuf)) ConfigSections.push_back(std::move(cs));
			}
		}
		// If this point is reached, file was successfully read; return success if any sections added to collection:
		return (ConfigSections.empty() == false);
	}
	catch(const std::exception&) {
		std::throw_with_nested(FORMAT_RUNTIME_ERROR("Exception reading configuration file"));
	}
}
