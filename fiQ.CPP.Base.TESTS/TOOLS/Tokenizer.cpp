#include "pch.h"
#include "CppUnitTest.h"
#include "Tools/Tokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(Tokenizer_TEST)
	{
	public:
		
		TEST_METHOD(TokenizeStrings)
		{
			// Tokenize string inline, using multiple delimiters:
			{char temp[50] = {0};
			strcpy_s(temp, "Field1,Field2|field3");
			Tokenizer toks = Tokenizer::CreateInline<10>(temp, strlen(temp), ",|");
			Assert::AreEqual(3ULL, toks.TokenCount(), L"Incorrect number of tokens read");
			Assert::AreEqual(6ULL, toks.Length(0), L"Token 0 invalid length");
			Assert::AreEqual("Field1", toks.Value(0), L"Token 0 incorrect value");
			Assert::AreEqual(6ULL, toks.Length(1), L"Token 1 invalid length");
			Assert::AreEqual("Field2", toks.Value(1), L"Token 1 incorrect value");
			Assert::AreEqual(6ULL, toks.Length(2), L"Token 2 invalid length");
			Assert::AreEqual("field3", toks.Value(2), L"Token 2 incorrect value");

			// Re-parse string inline using single delimiter:
			strcpy_s(temp, "Field4,Field5|field6");
			toks.AssignInline<10>(temp, strlen(temp), ',');
			Assert::AreEqual(2ULL, toks.TokenCount(), L"Incorrect number of tokens read");
			Assert::AreEqual(6ULL, toks.Length(0), L"Token 0 invalid length");
			Assert::AreEqual("Field4", toks.Value(0), L"Token 0 incorrect value");
			Assert::AreEqual(13ULL, toks.Length(1), L"Token 1 invalid length");
			Assert::AreEqual("Field5|field6", toks.Value(1), L"Token 1 incorrect value");}

			// Tokenize copy of string, using single delimiter:
			{const char temp[] = "Field1,Field2|field3!!!!!!!!";
			Tokenizer toks = Tokenizer::CreateCopy<10>(temp, 20ULL, ','); // Explicitly exclude last 8 chars of string from length
			Assert::AreEqual(2ULL, toks.TokenCount(), L"Incorrect number of tokens read");
			Assert::AreEqual(6ULL, toks.Length(0), L"Token 0 invalid length");
			Assert::AreEqual("Field1", toks.Value(0), L"Token 0 incorrect value");
			Assert::AreEqual(13ULL, toks.Length(1), L"Token 1 invalid length");
			Assert::AreEqual("Field2|field3", toks.Value(1), L"Token 1 incorrect value");

			// Re-parse from copy of string, using both delimiters and including full length of data:
			toks.AssignCopy<10>(temp, strlen(temp), ",|");
			Assert::AreEqual(3ULL, toks.TokenCount(), L"Incorrect number of tokens read");
			Assert::AreEqual(6ULL, toks.Length(0), L"Token 0 invalid length");
			Assert::AreEqual("Field1", toks.Value(0), L"Token 0 incorrect value");
			Assert::AreEqual(6ULL, toks.Length(1), L"Token 1 invalid length");
			Assert::AreEqual("Field2", toks.Value(1), L"Token 1 incorrect value");
			Assert::AreEqual(14ULL, toks.Length(2), L"Token 2 invalid length");
			Assert::AreEqual("field3!!!!!!!!", toks.Value(2), L"Token 2 incorrect value");}

			// Tests with std::string values:
			{Tokenizer toks = Tokenizer::CreateCopy<10>(std::string("Field1|Field2,Field3"));
			Assert::AreEqual(2ULL, toks.TokenCount(), L"Incorrect number of tokens read");
			Assert::AreEqual(13ULL, toks.Length(0), L"Token 0 invalid length");
			Assert::AreEqual("Field1|Field2", toks[0], L"Token 0 incorrect value");
			Tokenizer::Token token = toks.GetToken(1);
			Assert::AreEqual(6ULL, token.second, L"Token 1 incorrect length");
			Assert::AreEqual("Field3", token.first, L"Token 1 incorrect value");
			// Re-parse from new string
			toks.AssignCopy<10>(std::string("Field1,Field2"));
			Assert::AreEqual(2ULL, toks.TokenCount(), L"Incorrect number of tokens read");
			Assert::AreEqual(std::string("Field1"), toks.GetString(0), L"Token 0 incorrect value");}
		}

	};

}