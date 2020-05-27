#pragma once
//==========================================================================================================================
// LogMessageBuilder.h : Definition of classes and utilities for generating LogMessage objects
//==========================================================================================================================

#include "Logging/LogMessageTemplate.h"
#include "Logging/LogMessage.h"

namespace FIQCPPBASE {

//==========================================================================================================================
// LogMessageBuilder: Class used to combine compile-time template with run-time arguments to produce a LogMessage
template<size_t N, // Placeholder count dictated by LogMessageTemplate (provided separately for compile-time validation)
	typename T // Tuple of N logging arguments (wrapped and forwarded by helper function)
>
class LogMessageBuilder {
public:

	// Build: Apply parameters to template to construct a LogMessage object
	std::unique_ptr<const LogMessage> Build(LogMessage::ContextEntries&& context) const;

	constexpr LogMessageBuilder(LogLevel _level, const LogMessageTemplate& _lt, T&& t) noexcept
		: level(_level), lt(_lt), runtimeargs(t), escapeformats(_lt.EscapeFormats()) {
		// Ensure number of arguments received by constructor matches expected number of placeholders in
		// message template (done with static_assert instead of SFINAE to provide friendlier error):
		static_assert(std::tuple_size_v<T> == N, "Mismatch between number of arguments and template placeholders");
	}

private:

	// Private members - set at construction
	const LogLevel level; // Level associated with this message
	const LogMessageTemplate& lt; // Private reference to static format template for this log message
	const T runtimeargs; // Tuple of runtime arguments to be applied to template placeholder
	mutable FormatEscape escapeformats; // Formats for which LogMessage will require character escaping

	//======================================================================================================================
	// Element size calculation functions - used by ElementSize functions to determine approximate length of element:
	template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	static constexpr size_t CalcElementSize(const T&, size_t precision) {
		// Use value explicitly set in template, if any, otherwise max decimal digits for type:
		return (precision ? precision : StringOps::Decimal::MaxDigits_v<T>);
	}
	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	static constexpr size_t CalcElementSize(const T&, size_t precision) {
		// Floating point values will ultimately be written as one long integer, decimal point
		// plus number of decimal digits equal to precision (minimum 1):
		return StringOps::Decimal::MaxDigits_v<long long> + 1 + precision;
	}
	template<typename T, std::enable_if_t<std::is_same_v<std::string,T>, int> = 0>
	static constexpr size_t CalcElementSize(const T& t, size_t precision) {
		// Use exact length of string, up to precision:
		return (t.length() > precision ? precision : t.length());
	}
	template<typename T, std::enable_if_t<std::is_same_v<std::remove_const_t<T>, char>, int> = 0>
	static constexpr size_t CalcElementSize(T* t, size_t precision) {
		// Use length of string, up to precision:
		const size_t len = strlen(t);
		return (len > precision ? precision : len);
	}

	//======================================================================================================================
	// Element size totaling functions - used during Build determine approximate total length of message:
	template<size_t s, std::enable_if_t<(s > (N + N)), int> = 0> // Terminal case
	size_t ElementSize() const noexcept(false) {return 0;}
	template<size_t s, std::enable_if_t<(s <= (N + N) && (s % 2) != 0), int> = 0> // Placeholder case
	size_t ElementSize() const noexcept(false) {
		return CalcElementSize(std::get<s / 2>(runtimeargs), lt.PlaceholderPrecision(s / 2))
			+ ElementSize<s + 1>(); // Continue to next argument
	}
	template<size_t s, std::enable_if_t<(s <= (N + N) && (s % 2) == 0), int> = 0> // Fixed string case
	size_t ElementSize() const noexcept(false) {
		return lt.TokenLength(s) // Exact length of non-placeholder text
			+ ElementSize<s + 1>(); // Continue to next argument
	}
	size_t ElementSizes() const noexcept(false) {return ElementSize<0>();}

	// Element formatting functions - used during Build to format a single element:
	template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	static constexpr std::pair<const char*,size_t> FormatElement(
		_Out_writes_(LogMessageTemplate::MAX_PLACEHOLDER_LEN) char* buf, T t,
		LogMessageTemplate::Format format, size_t precision, FormatEscape&) {
		// Write integral value to preallocated buffer, returning buffer and bytes written:
		if(format == LogMessageTemplate::Format::Hex) return StringOps::Ascii::FlexWriteString(buf, t, precision);
		else if(precision > 0) return StringOps::Decimal::FlexWriteString(buf, t, precision);
		else return { buf, StringOps::Decimal::FlexWriteString(buf, t) };
	}
	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	static constexpr std::pair<const char*,size_t> FormatElement(
		_Out_writes_(LogMessageTemplate::MAX_PLACEHOLDER_LEN) char* buf, T t,
		LogMessageTemplate::Format, size_t precision, FormatEscape&) {
		// Write float value into preallocated buffer, returning buffer and bytes written:
		return StringOps::Float::FlexWriteString(buf, t, precision);
	}
	template<typename T, std::enable_if_t<std::is_same_v<std::string,T>, int> = 0>
	static constexpr std::pair<const char*,size_t> FormatElement(char*, // Preallocated buffer not required
		const T& t, LogMessageTemplate::Format, size_t precision, FormatEscape& escapeformats) {
		const size_t len = ValueOps::Bounded<size_t>(0, t.length(), precision);
		for(size_t s = 0; s < len; ++s) escapeformats |= StringOps::NeedsEscape(t.at(s));
		return { t.data(), len };
	}
	template<typename T, std::enable_if_t<std::is_same_v<std::remove_const_t<T>, char>, int> = 0>
	static constexpr std::pair<const char*,size_t> FormatElement(char*, // Preallocated buffer not required
		const T* t, LogMessageTemplate::Format, size_t precision, FormatEscape& escapeformats) {
		const size_t len = ValueOps::Bounded<size_t>(0, strlen(t), precision);
		for(size_t s = 0; s < len; ++s) escapeformats |= StringOps::NeedsEscape(t[s]);
		return { t, len };
	}

	//======================================================================================================================
	// Element construction functions - called during Build to apply template and parameters to output string and context
	template<size_t s, std::enable_if_t<(s > (N + N)), int> = 0> // Terminal case, end of tuple reached
	void BuildElement(char*, std::string&, LogMessage::ContextEntries&) const noexcept(false) {}
	template<size_t s, std::enable_if_t<(s <= (N + N) && (s % 2) != 0), int> = 0> // Placeholder case
	//GSL_SUPPRESS(con.4) // "arg" value below is sometimes pointer, sometimes not - can't always be const const
	void BuildElement(
		_Out_writes_(LogMessageTemplate::MAX_PLACEHOLDER_LEN) char* buf,
		std::string& message, LogMessage::ContextEntries& context) const noexcept(false) {
		// Retrieve argument from tuple, format and precision from template:
		const auto arg = std::get<s / 2>(runtimeargs);
		const auto phformat = lt.PlaceholderFormat(s / 2);
		const size_t phprecision = lt.PlaceholderPrecision(s / 2);
		// Format argument (using preallocated buffer, if required) and capture data/length:
		const std::pair<const char*,size_t> phvalue = FormatElement(buf, arg, phformat, phprecision, escapeformats);
		if(phvalue.second) { // Ignore zero-length values
			message.append(phvalue.first, phvalue.second); // Append to member string
			// If this is a named placeholder, add it to context collection:
			const size_t phnamelen = lt.PlaceholderLen(s / 2);
			if(phnamelen > 0) {
				context.emplace_back(std::piecewise_construct,
					std::forward_as_tuple(lt.Placeholder(s / 2), phnamelen),
					std::forward_as_tuple(phvalue.first, phvalue.second)
				);
			}
		}
		BuildElement<s + 1>(buf, message, context);
	}
	template<size_t s, std::enable_if_t<(s <= (N + N) && (s % 2) == 0), int> = 0> // Fixed string case
	void BuildElement(
		_Out_writes_(LogMessageTemplate::MAX_PLACEHOLDER_LEN) char* buf,
		std::string& message, LogMessage::ContextEntries& context) const noexcept(false) {
		message.append(lt.Token(s), lt.TokenLength(s));
		BuildElement<s + 1>(buf, message, context);
	}
	template<size_t bufsize>
	void BuildElements(char (&buf)[bufsize], std::string& message, LogMessage::ContextEntries& context) const noexcept(false) {
		static_assert(bufsize > LogMessageTemplate::MAX_PLACEHOLDER_LEN, "Write buffer too small");
		BuildElement<0>(buf, message, context);
	}

};

//==========================================================================================================================
// LogMessageBuilder<0>: Partial specialization of class for messages with zero placeholders (i.e. static log messages)
template<typename T> // Tuple of logging arguments (unused in this specialization)
class LogMessageBuilder<0,T> {
public:
	constexpr std::unique_ptr<const LogMessage> Build(LogMessage::ContextEntries&& context) const;
	constexpr LogMessageBuilder(LogLevel _level, const LogMessageTemplate& _lt) : level(_level), lt(_lt){}
private:
	const LogLevel level;
	const LogMessageTemplate& lt;
};


//==========================================================================================================================
// LogMessageBuilder<N>::Build: Construct LogMessage by applying runtime arguments to template:
template<size_t N, typename T>
inline std::unique_ptr<const LogMessage> LogMessageBuilder<N,T>::Build(LogMessage::ContextEntries&& context) const {
	// Create local string to hold data and pre-reserve expected length:
	std::string message;
	message.reserve(ElementSizes());
	// Create temporary buffer for quick formatting of elements, then apply elements to format:
	char temp[LogMessageTemplate::MAX_PLACEHOLDER_LEN + 5] = {0};
	BuildElements(temp, message, context);
	// Move locals into constructor of LogMessage and return:
	return LogMessage::Create(level, std::move(message), std::move(context), escapeformats);
}
// LogMessageBuilder<0>::Build: Construct LogMessage by directly passing static template string:
template<typename T>
inline constexpr std::unique_ptr<const LogMessage> LogMessageBuilder<0,T>::Build(LogMessage::ContextEntries&& context) const {
	return LogMessage::Create(level, lt.Token(0), lt.TokenLength(0), std::move(context), lt.EscapeFormats());
}

//==========================================================================================================================
// LogMessage argument validator: Ensures only valid types are passed to LogMessage as placeholder values
template<typename T>
struct ValidLogArgument : std::integral_constant<bool,
	std::is_integral_v<std::decay_t<T>>
	|| std::is_floating_point_v<std::decay_t<T>>
	|| std::is_same_v<std::decay_t<T>, char const*>
	|| std::is_same_v<std::decay_t<T>, char*>
	|| std::is_same_v<std::decay_t<T>, std::string>
> {};
template<typename...Args>
struct ValidLogArguments : std::conjunction<ValidLogArgument<Args>...> {};

//==========================================================================================================================
// Helper function to create a LogMessage object combining a compile-time formatting template and run-time arguments
// (receives token count of LogMessageTemplate via explicit template parameter in order to allow compile-time
// validation of arguments received against placeholders in template):
template<size_t PlaceholderCount, typename...Args>
const auto CreateLogMessageBuilder(LogLevel _level, const LogMessageTemplate& _lt, Args&&... args) noexcept {
	static_assert(ValidLogArguments<Args...>::value, "Invalid arguments found in list");
	return LogMessageBuilder<PlaceholderCount, std::tuple<Args...>>(_level, _lt, std::forward_as_tuple(args...));
}
// Partial specialization for creating builder with no runtime arguments
template<size_t N>
constexpr auto CreateLogMessageBuilder(LogLevel _level, const LogMessageTemplate& _lt) noexcept {
	return LogMessageBuilder<0,void>(_level, _lt);
}

}; // (end namespace FIQCPPBASE)
