#include "pch.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;

int main()
{
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try
	{
	}
	catch(const std::runtime_error& re) {
		printf("Caught runtime error:\n");
		auto m = Exceptions::UnrollExceptions(re);
		for(auto seek = m.rbegin(); seek != m.rend(); ++seek) {
			printf("%d %s\n", seek->first, seek->second.c_str());
		}
	}
	catch(const std::invalid_argument& ia) {
		printf("Caught invalid argument:\n");
		auto m = Exceptions::UnrollExceptions(ia);
		for(auto seek = m.rbegin(); seek != m.rend(); ++seek) {
			printf("%d %s\n", seek->first, seek->second.c_str());
		}
	}
	catch(const std::exception& e) {
		printf("Caught exception:\n");
		auto m = Exceptions::UnrollExceptions(e);
		for(auto seek = m.rbegin(); seek != m.rend(); ++seek) {
			printf("%d %s\n", seek->first, seek->second.c_str());
		}
	}
}
