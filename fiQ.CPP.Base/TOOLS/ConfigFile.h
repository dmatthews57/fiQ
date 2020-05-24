#pragma once
//==========================================================================================================================
// ConfigFile.h : Classes for loading and accessing configuration files
//==========================================================================================================================

#include "Tools/StringOps.h"
#include "Tools/Tokenizer.h"

namespace FIQCPPBASE {

class ConfigFile {
private: struct pass_key {}; // Private function pass-key definition
public:

	//======================================================================================================================
	// ConfigSection: Container class for collection of entries under particular heading
	class ConfigSection {
	private: struct pass_key {}; // Private function pass-key definition
	public:
		// Type definitions
		using StringView = std::pair<const char*,size_t>;

		//==================================================================================================================
		// External accessors and utilities
		_Check_return_ const std::string& GetSectionName() const noexcept {return SectionName;}
		_Check_return_ size_t GetEntryCount() const noexcept {return ConfigEntries.size();}
		_Check_return_ static constexpr bool BoolParm(char c) noexcept {return ValueOps::Is(c).InSet('T', 't', 'Y', 'y', '1');}
		_Check_return_ static bool BoolParm(_In_z_ const char* s) noexcept {return (s ? BoolParm(s[0]) : false);}

		//==================================================================================================================
		// GetNamed accessors: Retrieve entry with the specified name in various formats
		// - Each function provides versions taking string literal names (faster) or std::string-constructible names
		template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
		_Check_return_ std::string GetNamedString(_In_reads_z_(len - 1) T (&name)[len]) const;
		_Check_return_ std::string GetNamedString(const std::string& name) const;
		template<size_t MaxToks, typename T, size_t len, typename...Args,
			std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
		_Check_return_ Tokenizer GetNamedTokenizer(_In_reads_z_(len - 1) T (&name)[len], Args&&...delimiter) const;
		template<size_t MaxToks, typename...Args>
		_Check_return_ Tokenizer GetNamedTokenizer(const std::string& name, Args&&...delimiter) const;
		template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
		_Check_return_ int GetNamedInt(_In_reads_z_(len - 1) T (&name)[len]) const;
		_Check_return_ int GetNamedInt(const std::string& name) const;
		template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
		_Check_return_ unsigned short GetNamedUShort(_In_reads_z_(len - 1) T (&name)[len]) const;
		_Check_return_ unsigned short GetNamedUShort(const std::string& name) const;
		template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
		_Check_return_ unsigned long long GetNamedHex(_In_reads_z_(len - 1) T (&name)[len]) const;
		_Check_return_ unsigned long long GetNamedHex(const std::string& name) const;
		template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
		_Check_return_ bool GetNamedBool(_In_reads_z_(len - 1) T (&name)[len]) const;
		_Check_return_ bool GetNamedBool(const std::string& name) const;

		//==================================================================================================================
		// Iterator accessors: Allow ordered access to ConfigEntries collection
		// - Use of auto return type allows outside entities to "see" the private ConfigEntry class, but only access public
		//   members (essentially only the GetEntry() accessor member to retrieve raw unnamed data)
		// - Use these functions with care - anything that modifies this object's collection (or destructs this object) will
		//   obviously invalidate all iterators and lead to undefined behaviour (and they are not thread-safe)
		auto Begin() const noexcept {return ConfigEntries.cbegin();}
		auto End() const noexcept {return ConfigEntries.cend();}

		//==================================================================================================================
		// ReadSection: Read all lines from file pointer, populating this object until start of next section or end of file
		// - Declared as public so it can be called from ConfigFile::Initialize, but locked with ConfigFile-private passkey
		//   to ensure only ConfigFile can call it
		// - ReadBuf is provided as an input so single read buffer can be re-used by all sections; ReadBufSize is template
		//   argument to allow compile-time validation (needs to be converted to integer for compatibility with fread)
		// - Returns true if at least one entry read
		template<size_t ReadBufSize>
		_Check_return_ bool ReadSection(ConfigFile::pass_key, _Inout_ FILE* file, _Out_writes_(ReadBufSize) char* ReadBuf);

		//==================================================================================================================
		// Public constructor
		// - Declared as public (so it is accessible by make_shared) but locked with ConfigFile-private passkey to ensure
		//   only ConfigFile can actually call it
		// - Arguments are relayed directly to std::string constructor (enable_if required to ensure that compiler selects
		//   appropriate constructors for non-string argument types)
		template<typename...Args,
			std::enable_if_t<std::is_constructible_v<std::string, Args...>, int> = 0>
		ConfigSection(ConfigFile::pass_key, Args&&...args) : SectionName(std::forward<Args>(args)...) {}
		// Deleted copy/move constructors and assignment operators
		ConfigSection() = delete;
		ConfigSection(const ConfigSection&) = delete;
		ConfigSection(ConfigSection&&) = delete;
		ConfigSection& operator=(const ConfigSection&) = delete;
		ConfigSection& operator=(ConfigSection&&) = delete;
		~ConfigSection() = default;

	private:

		//==================================================================================================================
		// ConfigEntry: Container class for individual entry in configuration section
		class ConfigEntry {
		public:
			// Pure public functions: Get text or tokenized Entry value
			_Check_return_ const std::string& GetEntry() const noexcept {return Entry;}
			template<size_t MaxToks, typename...Args>
			_Check_return_ Tokenizer GetTokenizedEntry(Args&&...delimiter) const {
				return Tokenizer::CreateCopy<MaxToks>(Entry, std::forward<Args>(delimiter)...);
			}
			// Locked public functions (can only be executed from ConfigSection due to pass_key)
			_Check_return_ ConfigSection::StringView GetValueIfName(
				ConfigSection::pass_key, _In_reads_(namelen) const char* name, size_t namelen) const;
			ConfigEntry(ConfigSection::pass_key, _In_ const char *start, _In_ const char *end, _In_opt_ const char *equal);
			// Deleted copy/move constructors and assignment operators (note these functions are not problematic as this
			// object is easy to move/copy - but outer class is designed to avoid move/copy so these are deleted in order
			// to provide warning at compile-time if a logic error has led to them being copied/etc
			ConfigEntry() = delete;
			ConfigEntry(const ConfigEntry&) = delete;
			ConfigEntry(ConfigEntry&&) = delete;
			ConfigEntry& operator=(const ConfigEntry&) = delete;
			ConfigEntry& operator=(ConfigEntry&&) = delete;
			~ConfigEntry() = default;
		private:
			const std::string Entry;						// Full text of original source line
			const std::string::size_type SeparatorIndex;	// Location of "=" separator in "Name=Value" pair, if detected
			std::string EntryName;							// "Name" portion of "Name=Value" pair, if detected
		};

		//==================================================================================================================
		// GetNamedConfig: Private utility function to locate ConfigEntry by name and return its data value
		_Check_return_ StringView GetNamedConfig(_In_reads_(namelen) const char *name, size_t namelen) const;

		//==================================================================================================================
		// Private members
		_Check_return_ static const std::string& EmptyStr(); // Access empty string when needed
		std::deque<ConfigEntry> ConfigEntries;	// Collection of ConfigEntry objects
		const std::string SectionName;			// Name of this configuration section
	};
	using SectionPtr = std::shared_ptr<const ConfigSection>;

	//======================================================================================================================
	// Initialize: Open specified file and read into memory
	// - Returns true if file is successfully read and at least one section with one parameter was loaded
	// - Will re-throw any exceptions it encounters
	_Check_return_ bool Initialize(_In_z_ const char* FileName);
	// GetSection: Retrieve section with specified name (case-insensitive), string literal version
	// - Will return nullptr if section not found
	template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int> = 0>
	_Check_return_ SectionPtr GetSection(_In_reads_z_(len - 1) T (&SectionName)[len]) const;
	// GetSection: Retrieve section with specified name (case-insensitive), string version
	// - Will return nullptr if section not found
	_Check_return_ SectionPtr GetSection(const std::string& SectionName) const;

private:

	//======================================================================================================================
	// Private members
	std::deque<SectionPtr> ConfigSections;	// Collection of ConfigSection objects
};

//==========================================================================================================================
// ConfigEntry constructor: Store entire configuration line, check for "Name=Value" pair
inline ConfigFile::ConfigSection::ConfigEntry::ConfigEntry(
	ConfigSection::pass_key, _In_ const char *start, _In_ const char *end, _In_opt_ const char *equal)
	: Entry(start, end), SeparatorIndex(ValueOps::Is(equal).InRangeEx(start, end) ? (equal - start) : std::string::npos) {
	// If SeparatorIndex is a valid value, this may be a "Name=Value" pair:
	if(SeparatorIndex != std::string::npos) {
		const std::string::const_iterator Separator = Entry.cbegin() + SeparatorIndex;
		if(ValueOps::Is(Separator).InRangeEx(Entry.cbegin(), Entry.cend())) {
			// Store "name" value (right-trimmed) in local member variable:
			EntryName = StringOps::TrimRight(std::string(Entry.cbegin(), Separator));
			// If "Name" element is at least 2 chars long, check if it is encased in quotation marks:
			if(EntryName.length() >= 2) {
				if(EntryName.front() == '\"' && EntryName.back() == '\"') {
					EntryName = StringOps::Trim(std::string(EntryName.cbegin() + 1, (EntryName.crbegin() + 1).base()));
				}
			}
		}
	}
}
// ConfigEntry::GetValueIfName: If this parameter is a "Name=Value" pair with matching "Name", retrieve "Value" portion of data
// - Returns a StringView with location and length of value data; if Name does not match, pair will have null pointer/zero length
_Check_return_ inline ConfigFile::ConfigSection::StringView ConfigFile::ConfigSection::ConfigEntry::GetValueIfName(
	ConfigSection::pass_key, _In_reads_(namelen) const char* name, size_t namelen) const {
	// Only consider possible matches if local and input names are valid and same length, SeparatorIndex is set (cheapest checks):
	if((namelen > 0 && SeparatorIndex != std::string::npos) ? (namelen == EntryName.length()) : false) {
		// Calculate position of where "value" starts, ensure resulting iterator is valid:
		const std::string::const_iterator StartOfData = Entry.cbegin() + SeparatorIndex + 1;
		if(ValueOps::Is(StartOfData).InRangeEx(Entry.cbegin(), Entry.cend())) {
			// Run case-insensitive comparison between names (most-expensive); if they match, this is the correct parameter:
			if(_strnicmp(EntryName.data(), name, namelen) == 0) {

				// Set iterator at start of data [first non-whitespace character following separator; note if there are no
				// valid characters, NonBlank will be equal to Entry.cend(); also note Entry is already right-trimmed]:
				std::string::const_iterator NonBlank = std::find_if_not(
					StartOfData,
					Entry.cend(),
					[](int ch){return std::isspace(ch);}
				);

				// Return pair with location and length of data - if there are at least two characters in available data,
				// check whether value is wrapped in quotes (in which case, exclude them):
				if(ValueOps::Is(NonBlank).InRangeLeft(StartOfData, Entry.cend()) ?
					((Entry.cend() - NonBlank) >= 2 ? (NonBlank[0] == '\"' && Entry.back() == '\"') : false)
					: false) {
					return { &NonBlank[1], (Entry.cend() - NonBlank) - 2 };
				}
				// Special case - if NonBlank is at end of data (i.e. value is blank), it cannot be dereferenced; just
				// use empty string data, with zero length:
				else if(NonBlank == Entry.cend()) {
					return { EmptyStr().data(), 0 };
				}
				else return { &NonBlank[0], Entry.cend() - NonBlank };
			}
		}
	}
	// If this point is reached, name does not match (or this is not a named parameter); return null pair:
	return {nullptr, 0};
}

//==========================================================================================================================
// ConfigSection::GetNamedConfig: Private utility function to locate ConfigEntry by name and return its data value
// - Returns a StringView with location and length of Value data; if Name not found, pair will have null pointer
_Check_return_ inline ConfigFile::ConfigSection::StringView ConfigFile::ConfigSection::GetNamedConfig(
	_In_reads_(namelen) const char *name, size_t namelen) const {
	std::deque<ConfigEntry>::const_iterator cSeek = ConfigEntries.cbegin(), cEnd = ConfigEntries.cend();
	for(cSeek; cSeek != cEnd; ++cSeek) {
		// Retrieve value from this ConfigEntry; if result has valid pointer, return:
		const StringView retval = cSeek->GetValueIfName(pass_key{}, name, namelen);
		if(retval.first != nullptr) return retval;
	}
	// If this point is reached, config parameter was not found; return null pair:
	return { nullptr, 0 };
}
// ConfigSection::GetNamedString: Retrieve string parameter with the specified name (string literal version)
template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int>>
_Check_return_ inline std::string ConfigFile::ConfigSection::GetNamedString(_In_reads_z_(len - 1) T (&name)[len]) const {
	const StringView retval = GetNamedConfig(name, len - 1);
	return (retval.first != nullptr && retval.second > 0) ? std::string(retval.first, retval.second) : EmptyStr();
}
// ConfigSection::GetNamedString: Retrieve string parameter with the specified name (string version)
_Check_return_ inline std::string ConfigFile::ConfigSection::GetNamedString(const std::string& name) const {
	const StringView retval = GetNamedConfig(name.data(), name.length());
	return (retval.first != nullptr && retval.second > 0) ? std::string(retval.first, retval.second) : EmptyStr();
}
// ConfigSection::GetNamedTokenizer: Retrieve tokenized string parameter with the specified name (string literal version)
template<size_t MaxToks, typename T, size_t len, typename...Args, std::enable_if_t<std::is_same_v<T, const char>, int>>
_Check_return_ inline Tokenizer ConfigFile::ConfigSection::GetNamedTokenizer(
	_In_reads_z_(len - 1) T (&name)[len], Args&&...delimiter) const {
	const StringView retval = GetNamedConfig(name, len - 1);
	return Tokenizer::CreateCopy<MaxToks>(retval.first, retval.second, std::forward<Args>(delimiter)...);
}
// ConfigSection::GetNamedTokenizer: Retrieve tokenized string parameter with the specified name (string version)
template<size_t MaxToks, typename...Args>
_Check_return_ inline Tokenizer ConfigFile::ConfigSection::GetNamedTokenizer(
	const std::string& name, Args&&...delimiter) const {
	const StringView retval = GetNamedConfig(name.data(), name.length());
	return Tokenizer::CreateCopy<MaxToks>(retval.first, retval.second, std::forward<Args>(delimiter)...);
}
// ConfigSection::GetNamedInt: Retrieve integer parameter with the specified name (string literal version)
template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int>>
_Check_return_ inline int ConfigFile::ConfigSection::GetNamedInt(_In_reads_z_(len - 1) T (&name)[len]) const {
	const StringView retval = GetNamedConfig(name, len - 1);
	return (retval.first != nullptr && retval.second > 0) ? StringOps::Decimal::FlexReadString(retval.first, retval.second) : 0;
}
// ConfigSection::GetNamedInt: Retrieve integer parameter with the specified name (string version)
_Check_return_ inline int ConfigFile::ConfigSection::GetNamedInt(const std::string& name) const {
	const StringView retval = GetNamedConfig(name.data(), name.length());
	return (retval.first != nullptr && retval.second > 0) ? StringOps::Decimal::FlexReadString(retval.first, retval.second) : 0;
}
// ConfigSection::GetNamedUShort: Retrieve unsigned short parameter with the specified name (string literal version)
template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int>>
_Check_return_ inline unsigned short ConfigFile::ConfigSection::GetNamedUShort(_In_reads_z_(len - 1) T (&name)[len]) const {
	const StringView retval = GetNamedConfig(name, len - 1);
	if(retval.first != nullptr && retval.second > 0) {
		const int i = StringOps::Decimal::FlexReadString(retval.first, retval.second);
		return ValueOps::Is(i).InRange(0, 0xFFFF) ? gsl::narrow_cast<unsigned short>(i) : 0;
	}
	else return 0;
}
// ConfigSection::GetNamedUShort: Retrieve unsigned short parameter with the specified name (string version)
_Check_return_ inline unsigned short ConfigFile::ConfigSection::GetNamedUShort(const std::string& name) const {
	const StringView retval = GetNamedConfig(name.data(), name.length());
	if(retval.first != nullptr && retval.second > 0) {
		const int i = StringOps::Decimal::FlexReadString(retval.first, retval.second);
		return ValueOps::Is(i).InRange(0, 0xFFFF) ? gsl::narrow_cast<unsigned short>(i) : 0;
	}
	else return 0;
}
// ConfigSection::GetNamedHex: Retrieve unsigned integer from hex string parameter with the specified name (string literal version)
template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int>>
_Check_return_ inline unsigned long long ConfigFile::ConfigSection::GetNamedHex(_In_reads_z_(len - 1) T (&name)[len]) const {
	const StringView retval = GetNamedConfig(name, len - 1);
	return (retval.first != nullptr && retval.second > 0) ? StringOps::Ascii::FlexReadString(retval.first, retval.second) : 0;
}
// ConfigSection::GetNamedHex: Retrieve unsigned integer from hex string parameter with the specified name (string version)
_Check_return_ inline unsigned long long ConfigFile::ConfigSection::GetNamedHex(const std::string& name) const {
	const StringView retval = GetNamedConfig(name.data(), name.length());
	return (retval.first != nullptr && retval.second > 0) ? StringOps::Ascii::FlexReadString(retval.first, retval.second) : 0;
}
// ConfigSection::GetNamedBool: Retrieve boolean parameter with the specified name (string literal version)
// - Bases decision on first character only, as determined by BoolParm utility function
template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int>>
_Check_return_ inline bool ConfigFile::ConfigSection::GetNamedBool(_In_reads_z_(len - 1) T (&name)[len]) const {
	const StringView retval = GetNamedConfig(name, len - 1);
	return (retval.first != nullptr && retval.second > 0 ? BoolParm(retval.first[0]) : false);
}
// ConfigSection::GetNamedBool: Retrieve boolean parameter with the specified name (string version)
// - Bases decision on first character only, as determined by BoolParm utility function
_Check_return_ inline bool ConfigFile::ConfigSection::GetNamedBool(const std::string& name) const {
	const StringView retval = GetNamedConfig(name.data(), name.length());
	return (retval.first != nullptr && retval.second > 0 ? BoolParm(retval.first[0]) : false);
}

//==========================================================================================================================
// ConfigFile::GetSection: Retrieve ConfigSection object with the specified name
template<typename T, size_t len, std::enable_if_t<std::is_same_v<T, const char>, int>>
_Check_return_ inline ConfigFile::SectionPtr ConfigFile::GetSection(_In_reads_z_(len - 1) T (&SectionName)[len]) const {
	constexpr size_t namelen = len - 1;
	auto cSeek = ConfigSections.cbegin(), cEnd = ConfigSections.cend();
	for(cSeek; cSeek != cEnd; ++cSeek) {
		if(cSeek->get()->GetSectionName().length() == namelen) {
			if(_strnicmp(cSeek->get()->GetSectionName().data(), SectionName, namelen) == 0) return *cSeek;
		}
	}
	return nullptr;
}
// ConfigFile::GetSection: Retrieve ConfigSection object with the specified name
_Check_return_ inline ConfigFile::SectionPtr ConfigFile::GetSection(const std::string& SectionName) const {
	auto cSeek = ConfigSections.cbegin(), cEnd = ConfigSections.cend();
	for(cSeek; cSeek != cEnd; ++cSeek) {
		if(_stricmp(cSeek->get()->GetSectionName().c_str(), SectionName.c_str()) == 0) return *cSeek;
	}
	return nullptr;
}

}; // (end namespace FIQCPPBASE)
