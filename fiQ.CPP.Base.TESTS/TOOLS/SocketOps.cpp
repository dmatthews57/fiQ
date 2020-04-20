#include "pch.h"
#include "CppUnitTest.h"
#include "Tools/SocketOps.h"
#include <future>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FIQCPPBASE;

namespace Microsoft {
	namespace VisualStudio {
		namespace CppUnitTestFramework {
			template<>
			std::wstring ToString<SocketOps::Result>(const enum SocketOps::Result& rc) { return std::to_wstring(static_cast<int>(rc)); }
		}
	}
}

namespace fiQCPPBaseTESTS
{

	TEST_CLASS(SocketOps_TEST)
	{
	private:
		SocketOps::ServerSocketPtr Server;
		SocketOps::SessionSocketPtr ClientSession;
		SocketOps::SessionSocketPtr ServerSession;

	public:


		TEST_CLASS_INITIALIZE(Class_Init) // Executes before any TEST_METHODs
		{
			SocketOps::InitializeSockets(true);
			Logger::WriteMessage("Sockets initialized");
		}		

		TEST_METHOD_INITIALIZE(Method_Init) // Executes before each TEST_METHOD
		{
		}

		TEST_METHOD(BasicConnections)
		{
			// Open up listening socket:
			Server = SocketOps::ServerSocket::Create();
			Assert::IsTrue(Server->Open(11223), (L"Open: " + StringOps::ConvertToWideString(Server->GetLastErrString())).c_str());

			// Start client socket connection:
			ClientSession = SocketOps::SessionSocket::ConnectAsync("127.0.0.1", 11223);
			Assert::IsTrue(ClientSession->SocketValid(), (L"ConnectAsync: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Wait for and accept connection request on server:
			Assert::AreEqual(SocketOps::Result::OK, Server->WaitEvent(10), StringOps::ConvertToWideString(Server->GetLastErrString()).c_str());
			ServerSession = Server->Accept();
			Assert::IsTrue(ServerSession->SocketValid(), (L"Accept: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());

			// Complete connection request at client:
			Assert::AreEqual(SocketOps::Result::OK, ClientSession->PollConnect(), (L"PollConnect: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Send simple message from client:
			{const char sendbuf[] = "HELLO"; size_t bs = strlen(sendbuf);
			Assert::AreEqual(SocketOps::Result::OK, ClientSession->Send(sendbuf, bs), (L"Send: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Wait for and receive message at server (read available):
			char readbuf[64] = {0}; size_t br = 0;
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->WaitEvent(10), (L"Read wait: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->ReadAvailable(readbuf, 50, br), (L"Read: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(bs, br, L"Invalid number of bytes read");
			Assert::AreEqual(sendbuf, readbuf, L"Invalid message received");}

			// Send simple message from client:
			{const char sendbuf[] = "HELLO"; size_t bs = strlen(sendbuf);
			Assert::AreEqual(SocketOps::Result::OK, ClientSession->Send(sendbuf, bs), (L"Send: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Wait for and receive message at server (read exact):
			char readbuf[64] = {0};
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->WaitEvent(10), (L"Read wait: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->ReadExact(readbuf, bs, 100), (L"Read: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(sendbuf, readbuf, L"Invalid message received");}

			// Send packet format message from client:
			{const char sendbuf[] = "\x00\x05HELLO";
			Assert::AreEqual(SocketOps::Result::OK, ClientSession->Send(sendbuf, 7), (L"Packet send: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Wait for and receive packet at server:
			char readbuf[64] = {0}; size_t br = 0;
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->WaitEvent(10), (L"Packet wait: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->ReadPacket(readbuf, 50, br, 100), (L"Packet read: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(5ULL, br, L"Invalid number of bytes read");
			Assert::AreEqual("HELLO", readbuf, L"Invalid message received");}
		}

		TEST_METHOD(TLSConnections)
		{
			// Open up listening socket:
			Server = SocketOps::ServerSocket::Create();
			Assert::IsTrue(Server->Open(11223), (L"Open: " + StringOps::ConvertToWideString(Server->GetLastErrString())).c_str());
			Assert::IsTrue(Server->InitCredentialsFromStore("localhost", "", false), (L"InitCredentials: " + StringOps::ConvertToWideString(Server->GetLastErrString())).c_str());

			// Start client socket connection:
			ClientSession = SocketOps::SessionSocket::ConnectAsync("127.0.0.1", 11223, true);
			Assert::IsTrue(ClientSession->SocketValid(), (L"ConnectAsync: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Wait for and accept connection request on server:
			Assert::AreEqual(SocketOps::Result::OK, Server->WaitEvent(10), (L"Server wait: " + StringOps::ConvertToWideString(Server->GetLastErrString())).c_str());

			// Create threaded task to perform Accept (blocking call needs to be done in parallel with ClientSession->PollConnect):
			{std::packaged_task<bool ()> task([this] { ServerSession = Server->Accept(nullptr, 1000); return ServerSession->Valid(); });
			std::future<bool> result = task.get_future();
			std::thread th(std::move(task));

			// Complete connection request at client:
			Assert::AreEqual(SocketOps::Result::OK, ClientSession->PollConnect(100), (L"PollConnect: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Ensure that threaded server Accept function completed successfully and join thread:
			Assert::IsTrue(result.get(), (L"Accept: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			th.join();}

			Logger::WriteMessage(("CipherSuite: " + ServerSession->GetTLSCipherSuite()).c_str());

			// Send simple message from client:
			{const char sendbuf[] = "HELLO"; size_t bs = strlen(sendbuf);
			Assert::AreEqual(SocketOps::Result::OK, ClientSession->Send(sendbuf, bs), (L"Send: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Receive message at server (read available):
			char readbuf[64] = {0}; size_t br = 0;
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->WaitEvent(100), (L"Read wait: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->ReadAvailable(readbuf, 50, br), (L"Read: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(bs, br, L"Invalid number of bytes read");
			Assert::AreEqual(sendbuf, readbuf, L"Invalid message received");
			Logger::WriteMessage(readbuf);}

			// Send simple message from client:
			{const char sendbuf[] = "HELLO"; size_t bs = strlen(sendbuf);
			Assert::AreEqual(SocketOps::Result::OK, ClientSession->Send(sendbuf, bs), (L"Send: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Receive message at server (read exact):
			char readbuf[64] = {0};
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->WaitEvent(100), (L"Read wait: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->ReadExact(readbuf, bs, 100), (L"Read: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(sendbuf, readbuf, L"Invalid message received");
			Logger::WriteMessage(readbuf);}

			// Send packet format from client:
			{const char sendbuf[] = "\x00\x05HELLO";
			Assert::AreEqual(SocketOps::Result::OK, ClientSession->Send(sendbuf, 7), (L"Packet send: " + StringOps::ConvertToWideString(ClientSession->GetLastErrString())).c_str());

			// Receive packet at server:
			char readbuf[64] = {0}; size_t br = 0;
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->WaitEvent(100), (L"Packet wait: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(SocketOps::Result::OK, ServerSession->ReadPacket(readbuf, 50, br, 100), (L"Packet read: " + StringOps::ConvertToWideString(ServerSession->GetLastErrString())).c_str());
			Assert::AreEqual(5ULL, br, L"Invalid number of bytes read");
			Assert::AreEqual("HELLO", readbuf, L"Invalid message received");
			Logger::WriteMessage(readbuf);}
		}

		TEST_METHOD_CLEANUP(Method_Cleanup) // Executes after each TEST_METHOD
		{
			ClientSession.reset(nullptr);
			ServerSession.reset(nullptr);
			Server.reset(nullptr);
		}

		TEST_CLASS_CLEANUP(Class_Cleanup) // Executes after all TEST_METHODs
		{
			SocketOps::CleanupSockets();
			Logger::WriteMessage("Sockets cleaned up");
		}		

	};
}
