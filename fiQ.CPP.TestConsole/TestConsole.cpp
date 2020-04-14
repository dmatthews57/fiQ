#include "pch.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;

#include "Tools/Tokenizer.h"

int main()
{
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try
	{
		char temp1[50] = {0}; const char temp2[] = "Field1,Field2|field3!!!!!!!!";
		strcpy_s(temp1, "Field1,Field2|field3");
		Tokenizer toks1 = Tokenizer::CreateInline<10>(temp1, 20ULL, ",|");
		for(size_t i = 0; i < toks1.TokenCount(); ++i) printf("1 %zu [%s]\n", toks1.Length(i), toks1.Value(i));
		Tokenizer toks2 = Tokenizer::CreateCopy<10>(temp2, 20ULL, ',');
		for(size_t i = 0; i < toks2.TokenCount(); ++i) printf("2 %zu [%s]\n", toks2.Length(i), toks2.Value(i));
		//printf("%zu %zu\n", toks1.TokenCount(), toks2.TokenCount());

		toks2.AssignCopy<10>(std::string(temp2), ",|");
		//printf("%zu %zu\n", toks1.TokenCount(), toks2.TokenCount());
		for(size_t i = 0; i < toks2.TokenCount(); ++i) printf("3 %zu [%s]\n", toks2.Length(i), toks2.Value(i));

		std::vector<char> tst(temp1, temp1);
		if(tst.empty()) printf("Yes\n");
		tst.assign(temp1, temp1);
		if(tst.empty()) printf("Yes\n");
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
