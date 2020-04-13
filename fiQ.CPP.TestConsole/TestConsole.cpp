#include "pch.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;

#include "Tools/FileOps.h"

int main()
{
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try
	{
	}
	catch(const std::runtime_error& re)
	{
		printf("Caught runtime error [%s]\n", re.what());
	}
	catch(const std::invalid_argument& ia)
	{
		printf("Caught invalid argument [%s]\n", ia.what());
	}
}
