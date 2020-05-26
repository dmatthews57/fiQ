#include "pch.h"
#include "CppUnitTest.h"
#include "Messages/HSMRequest.h"
#include "Messages/MessageNode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace Microsoft {
	namespace VisualStudio {
		namespace CppUnitTestFramework {
			template<>
			std::wstring ToString<MessageNode::RouteResult>(const enum MessageNode::RouteResult& rr) { return std::to_wstring(static_cast<std::underlying_type_t<MessageNode::RouteResult>>(rr)); }
		}
	}
}

namespace fiQCPPBaseTESTS
{
	TEST_CLASS(FuturexHSMNode_TESTS)
	{
	private:
		static std::shared_ptr<MessageNode>& GetHSM() {
			static std::shared_ptr<MessageNode> hsm = MessageNode::Create("Futurex", 1, 1);
			return hsm;
		}

	public:

		TEST_CLASS_INITIALIZE(Class_Init) // Executes before any TEST_METHODs
		{
			Assert::AreEqual(true, GetHSM() != nullptr, L"HSM object not created");
			Assert::AreEqual(true, GetHSM()->Init(), L"HSM initialization failed");
			Logger::WriteMessage("HSM initialized");
		}		
	
		TEST_METHOD(GenerateKey)
		{
			std::shared_ptr<HSMRequest> h = HSMRequest::Create(
				HSMRequest::Operation::GenerateKey,
				HSMRequest::RequestFieldSet {
					{HSMRequest::FieldName::MFKMod, HSMLITERAL("D")},
					{HSMRequest::FieldName::KEK, HSMLITERAL("11223344556677889900AABBCCDDEEFF")},
					{HSMRequest::FieldName::KEKMod, HSMLITERAL("0")}
				}
			);
			Assert::AreEqual(MessageNode::RouteResult::Processed, GetHSM()->ProcessRequest(h), L"Request processing failed");

			{auto f = h->GetResponseField(HSMRequest::FieldName::KeyOutKEK);
			Assert::AreNotEqual(true, f.empty(), L"KeyOutKEK not retrieved");
			Assert::AreEqual(32ULL, f.length(), L"Invalid KeyOutKEK length");}

			{auto f = h->GetResponseField(HSMRequest::FieldName::KeyOutMFK);
			Assert::AreNotEqual(true, f.empty(), L"KeyOutMFK not retrieved");
			Assert::AreEqual(32ULL, f.length(), L"Invalid KeyOutMFK length");}

			{auto f = h->GetResponseField(HSMRequest::FieldName::KCVOut);
			Assert::AreNotEqual(true, f.empty(), L"KCVOut not retrieved");
			Assert::AreEqual(true, f.length() >= 4, L"Invalid KCVOut length");}
		}

		TEST_METHOD(TranslateKey)
		{
			std::shared_ptr<HSMRequest> h = HSMRequest::Create(
				HSMRequest::Operation::TranslateKey,
				HSMRequest::RequestFieldSet {
					{HSMRequest::FieldName::MFKMod, HSMLITERAL("D")},
					{HSMRequest::FieldName::KeyIn, HSMLITERAL("88888888888888888888888888888888")},
					{HSMRequest::FieldName::KEK, HSMLITERAL("11223344556677889900AABBCCDDEEFF")},
					{HSMRequest::FieldName::KEKMod, HSMLITERAL("D")}
				}
			);
			Assert::AreEqual(MessageNode::RouteResult::Processed, GetHSM()->ProcessRequest(h), L"Request processing failed");

			{auto f = h->GetResponseField(HSMRequest::FieldName::KeyOutMFK);
			Assert::AreNotEqual(true, f.empty(), L"KeyOutMFK not retrieved");
			Assert::AreEqual(32ULL, f.length(), L"Invalid KeyOutMFK length");}

			{auto f = h->GetResponseField(HSMRequest::FieldName::KCVOut);
			Assert::AreNotEqual(true, f.empty(), L"KCVOut not retrieved");
			Assert::AreEqual(true, f.length() >= 4, L"Invalid KCVOut length");}
		}

		TEST_METHOD(TranslatePIN)
		{
			std::shared_ptr<HSMRequest> h = HSMRequest::Create(
				HSMRequest::Operation::TranslatePIN,
				HSMRequest::RequestFieldSet {
					{HSMRequest::FieldName::PEKSrc, HSMLITERAL("88888888888888888888888888888888")},
					{HSMRequest::FieldName::PEKDst, HSMLITERAL("88888888888888888888888888888888")},
					{HSMRequest::FieldName::PINIn, HSMLITERAL("0123456789ABCDEF")},
					{HSMRequest::FieldName::PAN, HSMLITERAL("4219730010000001")}
				}
			);
			Assert::AreEqual(MessageNode::RouteResult::Processed, GetHSM()->ProcessRequest(h), L"Request processing failed");

			{auto f = h->GetResponseField(HSMRequest::FieldName::PINOut);
			Assert::AreNotEqual(true, f.empty(), L"PINOut not retrieved");
			Assert::AreEqual(16ULL, f.length(), L"Invalid PINOut length");}
		}

		TEST_CLASS_CLEANUP(Class_Cleanup) // Executes after all TEST_METHODs
		{
			Assert::AreEqual(true, GetHSM()->Cleanup(), L"HSM cleanup failed");
			GetHSM() = nullptr;
			Logger::WriteMessage("HSM cleaned up");
		}
	};
}