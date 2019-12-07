#include "header.h"
#include "ClientSession.h"
#include "SessionManager.h"
#include "IOContext.h"
#include "IocpManager.h"

IocpManager* iocpManager = nullptr;

LPFN_ACCEPTEX IocpManager::fnAcceptEx = nullptr;
LPFN_CONNECTEX IocpManager::fnConnectEx = nullptr; 
LPFN_DISCONNECTEX IocpManager::fnDisconnectEx = nullptr;
char IocpManager::acceptBuffer[64]{ 0, };

BOOL AcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, PVOID lpOutputBuffer, DWORD dwReceiveDataLength,
	DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped)
{
	return IocpManager::fnAcceptEx(sListenSocket, sAcceptSocket, lpOutputBuffer, dwReceiveDataLength,
		dwLocalAddressLength, dwRemoteAddressLength, lpdwBytesReceived, lpOverlapped);
}

BOOL ConnectEx(SOCKET hSocket, const struct sockaddr* name, int namelen, PVOID lpSendBuffer, DWORD dwSendDataLength, LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped)
{
	return IocpManager::fnConnectEx(hSocket, name, namelen, lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
}

BOOL DisconnectEx(SOCKET socket, LPOVERLAPPED overlapped, DWORD flags, DWORD reserved)
{
	return IocpManager::fnDisconnectEx(socket, overlapped, flags, reserved);
}

IocpManager::IocpManager()	:
	completionPortHandle(nullptr),
	threadHandles(nullptr),
	standbySocket(NULL),
	ioThreadCount(0)
{
}

IocpManager::~IocpManager() 
{
	if (threadHandles != nullptr)
		delete[] threadHandles;
}

bool IocpManager::InitializeIocp(unsigned short port)
{
	/// set num of I/O threads
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	ioThreadCount = static_cast<int>(systemInfo.dwNumberOfProcessors);
	threadHandles = new HANDLE[ioThreadCount];

	/// winsock initializing
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return false;

	/// Create I/O Completion Port
	if (nullptr == (completionPortHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)))
		return false;

	/// create TCP socket
	if (INVALID_SOCKET == (standbySocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)))
		return false;

	//make Completion Port
	if (completionPortHandle != CreateIoCompletionPort(reinterpret_cast<HANDLE>(standbySocket), completionPortHandle, 0, 0))
	{
		std::cout << "listen socket IOCP register error: " << GetLastError();
		return false;
	}

	//set socket option : reuse Address
	int opt{ 1 };
	if (SOCKET_ERROR == (setsockopt(standbySocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(int))))
	{
		std::cout << "setsockop() SO_REUSEADDR error: " << GetLastError();
		return false;
	}

	/// bind
	SOCKADDR_IN serveraddress;
	memset(&serveraddress,0, sizeof(SOCKADDR_IN));
	serveraddress.sin_family = AF_INET;
	serveraddress.sin_port = htons(port);
	serveraddress.sin_addr.s_addr = htonl(INADDR_ANY);
	if (SOCKET_ERROR == bind(standbySocket, reinterpret_cast<SOCKADDR*>(&serveraddress), sizeof(serveraddress)))
		return false;

	///make enable AceeptEx and DisconnectEx function 
	GUID acceptExGuid = WSAID_ACCEPTEX;
	DWORD bytes{0};
	if (SOCKET_ERROR == WSAIoctl(standbySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptExGuid, sizeof(GUID), &fnAcceptEx, sizeof(LPFN_ACCEPTEX), &bytes, nullptr, nullptr))
		return false;

	GUID disconnectExGuid = WSAID_DISCONNECTEX;
	if (SOCKET_ERROR == WSAIoctl(standbySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &disconnectExGuid, sizeof(GUID), &fnDisconnectEx, sizeof(LPFN_DISCONNECTEX), &bytes, nullptr, nullptr))
		return false;

	GUID connectExGuid = WSAID_CONNECTEX;
	if (SOCKET_ERROR == WSAIoctl(standbySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &connectExGuid, sizeof(GUID), &fnConnectEx, sizeof(LPFN_CONNECTEX), &bytes, nullptr, nullptr))
		return false;

	std::cout << "Start IOCP Server" << "\n";

	/// make session pool
	sessionManager->PrepareSessions();

	return true;
}

bool IocpManager::StartIoThreads()
{
	for (int i = 0; i < ioThreadCount; i++)	/// I/O Thread
	{
		DWORD threadId{0};
		HANDLE threadHandle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, IoWorkerThread, nullptr, 0, reinterpret_cast<unsigned int*>(&threadId)));
		if (threadHandle == nullptr)
			return false;
		else
			threadHandles[i] = threadHandle;
	}

	return true;
}

bool IocpManager::StartAccept()
{
	/// listen
	if (SOCKET_ERROR == listen(standbySocket, SOMAXCONN))
	{
		std::cout << "Listen Error\n";
		return false;
	}
		
	while (sessionManager->AcceptSessions())	//Prevent Preemtion
	{
		Sleep(100); 
	}

	return true;
}


void IocpManager::CleanupIocp()
{
	CloseHandle(completionPortHandle);

	int count = ioThreadCount;
	for (register int i = 0; i < count; ++i)
		CloseHandle(threadHandles[i]);

	WSACleanup();
}

unsigned int WINAPI IocpManager::IoWorkerThread(LPVOID lpParam)
{
	HANDLE hComletionPort = iocpManager->GetComletionPort();

	while (true)
	{
		DWORD transferred{ 0 };
		OverlappedIOContext* context{ nullptr };
		ULONG_PTR completionKey{ 0 };

		int retval = GetQueuedCompletionStatus(hComletionPort, &transferred, &completionKey, reinterpret_cast<LPOVERLAPPED*>(&context), INFINITE);

		ClientSession* theSession = (context == nullptr) ? nullptr : context->sessionObject;
		if (retval == 0 || transferred == 0)
		{
			if (GetLastError() == WAIT_TIMEOUT)
				continue;
		
			if (context->ioType == IOType::IO_RECV || context->ioType == IOType::IO_SEND )
			{
#if _DEBUG
				//assert(nullptr != theSession);
#else
				if (nullptr != theSession) exit(-1);
#endif
				theSession->PostDisconnect(DisconnectReason::DR_COMPLETION_ERROR);
				DeleteIoContext(context);
				continue;
			}
		}
	
		bool completionOk{ false };
		switch (context->ioType)
		{
		case IOType::IO_ACCEPT:
			completionOk = acceptCompletion(theSession, static_cast<OverlappedAcceptContext*>(context));
			break;

		case IOType::IO_RECV:
			completionOk = receiveCompletion(theSession, static_cast<OverlappedRecvContext*>(context), transferred);
			break;

		case IOType::IO_SEND:
			completionOk = sendCompletion(theSession, static_cast<OverlappedSendContext*>(context), transferred);
			break;

		case IOType::IO_DISCONNECT:
			completionOk = disconnectCompletion(theSession, static_cast<OverlappedDisconnectContext*>(context)->disconnectReason);
			break;

		default:
			std::cout << "Unknown I/O Type: " << static_cast<int>(context->ioType) << "\n";
			break;
		}

		if ( !completionOk )
			theSession->PostDisconnect(DisconnectReason::DR_IO_REQUEST_ERROR);

		DeleteIoContext(context);
	}

	return 0;
}

bool IocpManager::acceptCompletion(ClientSession* session, OverlappedAcceptContext* context)
{
	session->AcceptCompletion();

	return session->PostSend();
}

bool IocpManager::receiveCompletion(ClientSession* session, OverlappedRecvContext* context, DWORD transferred)
{
	session->RecvCompletion(transferred);

	return session->PostSend();	/// echo back Request Result
}

bool IocpManager::sendCompletion(ClientSession* session, OverlappedSendContext* context, DWORD transferred)
{
	session->SendCompletion(transferred);

	if (context->wsaBuf.len != transferred)
	{
		std::cout << "Partial SendCompletion requested [" << context->wsaBuf.len << "], sent [" << transferred << "]";
		return false;
	}

	return session->PostRecv();
}

bool IocpManager::disconnectCompletion(ClientSession* session, DisconnectReason dr)
{
	session->DisconnectCompletion(dr);

	return true;
}