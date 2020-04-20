#include "pch.h"
#include "Tools/Exceptions.h"
using namespace FIQCPPBASE;

#include "Tools/SocketOps.h"
#include <future>

;
;
//bool DoAccept() {
//	serversess = server->Accept(nullptr, 10000);
//	return serversess->Valid();
//}

int main()
{
	_set_invalid_parameter_handler(Exceptions::InvalidParameterHandler);
	_set_se_translator(Exceptions::StructuredExceptionTranslator);
	SetUnhandledExceptionFilter(&Exceptions::UnhandledExceptionFilter);

	try
	{
		SocketOps::InitializeSockets(true);

		SocketOps::ServerSocketPtr server = SocketOps::ServerSocket::Create();
		if(server->Open(11223)) {
			if(server->InitCredentialsFromStore("localhost", "", false)) {
				SocketOps::SessionSocketPtr client = SocketOps::SessionSocket::ConnectAsync("127.0.0.1", 11223, true);
				SocketOps::Result rc;
				if(SocketOps::ResultOK((rc = server->WaitEvent(5)))) {

					SocketOps::SessionSocketPtr serversess;
					std::packaged_task<bool ()> task([&] { serversess = server->Accept(nullptr, 100); return serversess->Valid(); });
					std::future<bool> result = task.get_future();
					std::thread th(std::move(task));

					if(SocketOps::ResultOK((rc = client->PollConnect(100)))) {
						const bool sessvalid = result.get();
						th.join();
						if(sessvalid) {
							printf("Session established: %s\n", client->GetTLSCipherSuite().c_str());
							if(SocketOps::ResultOK((rc = client->Send("HELLO", 5)))) {
								if(SocketOps::ResultOK((rc = serversess->WaitEvent(100)))) {
									char readbuf[64] = {0}; size_t br = 0;
									if(SocketOps::ResultOK((rc = serversess->ReadAvailable(readbuf, 50, br)))) {
										printf("Server session received: %zu bytes [%.*s]\n", br, gsl::narrow_cast<int>(br), readbuf);
									}
									else printf("Server session read failed [%d][%s]\n", rc, serversess->GetLastErrString().c_str());
								}
								else printf("Server session wait failed [%d][%s]\n", rc, serversess->GetLastErrString().c_str());
							}
							else printf("Client send failed [%d][%s]\n", rc, client->GetLastErrString().c_str());
						}
						else printf("Server accept failed [%s]\n", server->GetLastErrString().c_str());
					}
					else printf("Client poll failed [%d][%s]\n", rc, client->GetLastErrString().c_str());
				}
				else printf("Server wait failed [%d][%s]\n", rc, server->GetLastErrString().c_str());
			}
			else printf("Server TLS init failed [%s]\n", server->GetLastErrString().c_str());
		}
		else printf("Server init failed [%s]\n", server->GetLastErrString().c_str());
		server.reset(nullptr);
		SocketOps::CleanupSockets();
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
