#include "pch.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;

#include "Tools/SocketOps.h"

int main()
{
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);
	SocketOps::InitializeSockets(false);

	try
	{
		std::string LastError; char ip[20] = {0};
		if(SocketOps::DNSLookup("www.google.ca", ip, &LastError)) printf("%s\n", ip);
		printf("%d\n%d\n", INT_MAX, (std::numeric_limits<int>::max)());
	}
	catch(const std::runtime_error& re)
	{
		printf("Caught runtime error [%s]\n", re.what());
	}
	catch(const std::invalid_argument& ia)
	{
		printf("Caught invalid argument [%s]\n", ia.what());
	}

	SocketOps::CleanupSockets();
}
