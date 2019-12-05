#include "header.h"
#include "IOContext.h"
#include "ClientSession.h"
#include "IocpManager.h"
#include "SessionManager.h"
#include "CriticalSectionSync.h"


ClientSession::ClientSession() :
	connected(0),
	refCount(0)
{
	memset(&sessionAddress, 0, sizeof(SOCKADDR_IN));
	sessionSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
}

ClientSession::~ClientSession() {}

void ClientSession::ResetSession()
{
	CriticalSectionSync sync;

	connected = refCount = 0;
	memset(&sessionAddress, 0, sizeof(SOCKADDR_IN));

	LINGER lingerOption;
	lingerOption.l_onoff = 1;
	lingerOption.l_linger = 0;

	/// no TCP TIME_WAIT
	if (SOCKET_ERROR == setsockopt(sessionSocket, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&lingerOption), static_cast<int>(sizeof(LINGER))))
	{
		std::cout << "setsockopt linger option error : " << GetLastError() << "\n";
	}

	closesocket(sessionSocket);

	sessionSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
}

bool ClientSession::PostAccept()
{
	CriticalSectionSync sync;

	OverlappedAcceptContext* acceptContext = new OverlappedAcceptContext(this);
	DWORD bytes{ 0 };

	acceptContext->wsaBuf.len = 0;
	acceptContext->wsaBuf.buf = nullptr;
	
	if (false == AcceptEx(*iocpManager->GetStandbySocket(), sessionSocket, iocpManager->acceptBuffer, 0, static_cast<DWORD>(sizeof(SOCKADDR_IN)) + 16,
		static_cast<DWORD>(sizeof(SOCKADDR_IN)) + 16, &bytes, reinterpret_cast<LPOVERLAPPED>(acceptContext)))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			DeleteIoContext(acceptContext);
			std::cout << "AcceptEx Error : " << GetLastError() << "\n";
			return false;
		}
	}

	return true;
}

void ClientSession::AcceptCompletion()
{
	CriticalSectionSync sync;

	if (1 == InterlockedExchange(&connected, 1))
		return;/// already exists?

	bool resultOk{ true };

	do
	{
		if (SOCKET_ERROR == setsockopt(sessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char*>(iocpManager->GetStandbySocket()), sizeof(SOCKET)))
		{
			std::cout << "SO_UPDATE_ACCEPT_CONTEXT error: " << GetLastError() << "\n";
			resultOk = false;
			break;
		}

		int opt{ 1 };
		if (SOCKET_ERROR == setsockopt(sessionSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt), sizeof(int)))
		{
			std::cout << "set TCP_NODELAY error: " << GetLastError() << "\n";
			resultOk = false;
			break;
		}

		opt = 0;
		if (SOCKET_ERROR == setsockopt(sessionSocket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&opt), sizeof(int)))
		{
			std::cout << "set SO_RCVBUF change error: " << GetLastError() << "\n";
			resultOk = false;
			break;
		}

		int addrlen = sizeof(SOCKADDR_IN);
		if (SOCKET_ERROR == getpeername(sessionSocket, reinterpret_cast<SOCKADDR*>(&sessionAddress), &addrlen))
		{
			std::cout << "getpeername error: " << GetLastError() << "\n";
			resultOk = false;
			break;
		}

		HANDLE handle = CreateIoCompletionPort(reinterpret_cast<HANDLE>(sessionSocket), iocpManager->GetComletionPort(), reinterpret_cast<ULONG_PTR>(this), 0);
		if (handle != iocpManager->GetComletionPort())
		{
			std::cout << "CreateIoCompletionPort error: " << GetLastError() << "\n";
			resultOk = false;
			break;
		}
	} while (false);


	if (!resultOk)
	{
		DisconnectRequest(DisconnectReason::DR_ONCONNECT_ERROR);
		return;
	}

	std::cout << "Client Connected IP=" << inet_ntoa(sessionAddress.sin_addr) << ", PORT=" << ntohs(sessionAddress.sin_port) << "\n";
}

void ClientSession::DisconnectRequest(DisconnectReason dr)
{
	CriticalSectionSync sync;

	/// Already Disconnected or disConnecting
	if (0 == InterlockedExchange(&connected, 0)) return;

	OverlappedDisconnectContext* context = new OverlappedDisconnectContext(this, dr);

	if (false == DisconnectEx(sessionSocket, reinterpret_cast<LPWSAOVERLAPPED>(context), TF_REUSE_SOCKET, 0))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(context);
			std::cout << "ClientSession::DisconnectRequest Error : " << GetLastError() << "\n";
		}
	}
}

void ClientSession::DisconnectCompletion(DisconnectReason dr)
{
	CriticalSectionSync sync;

	std::cout << "Client Disconnected: Reason=" << static_cast<int>(dr) << " IP=" << inet_ntoa(sessionAddress.sin_addr) << ", PORT=" << ntohs(sessionAddress.sin_port) << "\n";
	ReleaseRef();	/// release refcount when added at issuing a session
}

bool ClientSession::PostRecv()
{
	CriticalSectionSync sync;

	if (!IsConnected())	return false;

	OverlappedRecvContext* recvContext = new OverlappedRecvContext(this);

	DWORD recvbytes{ 0 };
	DWORD flags{ 0 };
	recvContext->wsaBuf.len = static_cast<ULONG>(bufferSize);
	recvContext->wsaBuf.buf = recvBuffer;

	/// start real recv
	if (SOCKET_ERROR == WSARecv(sessionSocket, &recvContext->wsaBuf, 1, &recvbytes, &flags, reinterpret_cast<LPWSAOVERLAPPED>(recvContext), nullptr))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(recvContext);
			std::cout << "ClientSession::PostRecv Error : " << GetLastError() << "\n";
			return false;
		}
	}

	return true;
}

void ClientSession::RecvCompletion(DWORD transferred)
{
	CriticalSectionSync sync;
}

bool ClientSession::PostSend()
{
	CriticalSectionSync sync;

	if (!IsConnected())
		return false;

	OverlappedSendContext* sendContext = new OverlappedSendContext(this);

	DWORD sendbytes{ 0 };
	DWORD flags{ 0 };
	sendContext->wsaBuf.len = static_cast<ULONG>(bufferSize);
	sendContext->wsaBuf.buf = sendBuffer;

	/// start async send
	if (SOCKET_ERROR == WSASend(sessionSocket, &sendContext->wsaBuf, 1, &sendbytes, flags, (LPWSAOVERLAPPED)sendContext, nullptr))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(sendContext);
			std::cout << "ClientSession::PostSend Error : " << GetLastError() << "\n";

			return false;
		}
	}

	return true;
}

void ClientSession::SendCompletion(DWORD transferred)
{
	CriticalSectionSync sync;
}


void ClientSession::AddRef()
{
	CriticalSectionSync sync;

#ifdef _DEBUG
	//assert(InterlockedIncrement(&refCount) > 0);
#else
	if (InterlockedIncrement(&refCount) < 0)
		exit(-1);
#endif
}

void ClientSession::ReleaseRef()
{
	CriticalSectionSync sync;

	long ret = InterlockedDecrement(&refCount);
#ifdef _DEBUG
	//assert(ret >= 0);
#else
	if (ret < 0)
		exit(-1);
#endif
	if (ret == 0)
		sessionManager->ReturnClientSession(this);
}
