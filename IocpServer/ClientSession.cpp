#include "header.h"
#include "ClientSession.h"
#include "IocpManager.h"
#include "SessionManager.h"


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
	connected = refCount = 0;
	memset(&sessionAddress, 0, sizeof(SOCKADDR_IN));

	LINGER lingerOption;
	lingerOption.l_onoff = 1;
	lingerOption.l_linger = 0;

	/// no TCP TIME_WAIT
	if (SOCKET_ERROR == setsockopt(sessionSocket, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&lingerOption), static_cast<int>(sizeof(LINGER))))
		logManager->WriteLog("[DEBUG] setsockopt linger option error: %d\n", GetLastError());

	closesocket(sessionSocket);

	sessionSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
}

bool ClientSession::PostAccept()
{
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
			logManager->WriteLog("AcceptEx Error : %d\n", GetLastError());
			return false;
		}
	}

	return true;
}

void ClientSession::AcceptCompletion()
{
	CriticalSectionSync sync(cs);

	if (1 == InterlockedExchange(&connected, 1))
		return;/// already exists?

	bool resultOk{ true };

	do
	{
		if (SOCKET_ERROR == setsockopt(sessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char*>(iocpManager->GetStandbySocket()), sizeof(SOCKET)))
		{
			logManager->WriteLog("[DEBUG] SO_UPDATE_ACCEPT_CONTEXT error: %d", GetLastError());
			resultOk = false;
			break;
		}

		int opt{ 1 };
		if (SOCKET_ERROR == setsockopt(sessionSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt), sizeof(int)))
		{
			logManager->WriteLog("[DEBUG] TCP_NODELAY error: %d", GetLastError());
			resultOk = false;
			break;
		}

		opt = 0;
		if (SOCKET_ERROR == setsockopt(sessionSocket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&opt), sizeof(int)))
		{
			logManager->WriteLog("[DEBUG] SO_RCVBUF change error: %d", GetLastError());
			resultOk = false;
			break;
		}

		int addrlen = sizeof(SOCKADDR_IN);
		if (SOCKET_ERROR == getpeername(sessionSocket, reinterpret_cast<SOCKADDR*>(&sessionAddress), &addrlen))
		{
			logManager->WriteLog("[DEBUG] getpeername error: %d", GetLastError());
			resultOk = false;
			break;
		}

		HANDLE handle = CreateIoCompletionPort(reinterpret_cast<HANDLE>(sessionSocket), iocpManager->GetComletionPort(), reinterpret_cast<ULONG_PTR>(this), 0);
		if (handle != iocpManager->GetComletionPort())
		{
			logManager->WriteLog("[DEBUG] CreateIoCompletionPort error: %d", GetLastError());
			resultOk = false;
			break;
		}
	} while (false);


	if (!resultOk)
	{
		DisconnectRequest(DisconnectReason::DR_ONCONNECT_ERROR);
		return;
	}

	logManager->WriteLog("[DEBUG] Client Connected: IP=%s, PORT=%d", inet_ntoa(sessionAddress.sin_addr), GetPort());
}

bool ClientSession::Connect(const char* ipAddr, unsigned int port, SessionType type)
{
	CriticalSectionSync sync(cs);

	if (IsConnected())
	{
		printf("Error : Already Connected \n");
		return false;
	}
	
	memset(&sessionAddress, 0, sizeof(SOCKADDR_IN));
	sessionAddress.sin_addr.S_un.S_addr = inet_addr(ipAddr);
	sessionAddress.sin_port = htons(port);
	sessionAddress.sin_family = AF_INET;

	OverlappedConnectContext* context = new OverlappedConnectContext(this);
	
	if (false == ConnectEx(sessionSocket, reinterpret_cast<SOCKADDR*>(&sessionAddress), sizeof(SOCKADDR_IN), nullptr, 0, nullptr, reinterpret_cast<LPOVERLAPPED>(context)))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			DeleteIoContext(context);
			printf("Error : Start Connect : %d \n", GetLastError());
		}
	}

	sessionType = type;
	return true;
}

void ClientSession::ConnectCompletion()
{
	CriticalSectionSync sync(cs);

	printf("session Success to Connect Server\n");

	if (SOCKET_ERROR == setsockopt(sessionSocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0))
	{

		if (WSAENOTCONN == GetLastError())
			printf("Connection Server Failed -> WSAENDTCONN\n");
		else
			printf("SO_UPDATE_CONNECT_CONTEXT Failed  : %d\n", GetLastError());

		return;
	}

	int opt{ 1 };
	if (SOCKET_ERROR == setsockopt(sessionSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt), sizeof(int)))
	{
		printf("TCP_NODELAY change error  : %d\n", GetLastError());
		return;
	}

	opt = 0;
	if (SOCKET_ERROR == setsockopt(sessionSocket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&opt), sizeof(int)))
	{
		printf("SO_RCVBUF change error  : %d\n", GetLastError());
		return;
	}

	if (1 == InterlockedExchange(&connected, 1))
	{
		assert(false);
	}
}

bool ClientSession::ConvertToConnectionSession()
{
	CriticalSectionSync sync(cs);
	
	ResetSession();

	SOCKADDR_IN address;
	memset(&address, 0, sizeof(SOCKADDR_IN));
	address.sin_family = AF_INET;
	address.sin_addr.S_un.S_addr = INADDR_ANY;
	address.sin_port = 0;

	if (SOCKET_ERROR == bind(sessionSocket, reinterpret_cast<SOCKADDR*>(&address), sizeof(SOCKADDR_IN)))
	{
		printf("Bind Error : %d\n", GetLastError());
		return false;
	}

	if (iocpManager->GetComletionPort() != CreateIoCompletionPort(reinterpret_cast<HANDLE>(sessionSocket), iocpManager->GetComletionPort(), reinterpret_cast<ULONG_PTR>(this), 0))
	{
		printf("Session Completion Prepare Error : %d\n", GetLastError());
		return false;
	}

	return true;
}

void ClientSession::DisconnectRequest(DisconnectReason dr)
{
	/// Already Disconnected or disConnecting
	if (0 == InterlockedExchange(&connected, 0)) return;

	OverlappedDisconnectContext* context = new OverlappedDisconnectContext(this, dr);

	if (false == DisconnectEx(sessionSocket, reinterpret_cast<LPWSAOVERLAPPED>(context), TF_REUSE_SOCKET, 0))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(context);
			logManager->WriteLog("ClientSession::DisconnectRequest Error : %d", GetLastError());
		}
	}
}

void ClientSession::DisconnectCompletion(DisconnectReason dr)
{
	logManager->WriteLog("[DEBUG] Client Disconnected: Reason=%d IP=%s, PORT=%d", dr, inet_ntoa(sessionAddress.sin_addr), GetPort());
	ReleaseRef();	/// release refcount when added at issuing a session
}

bool ClientSession::PostRecv()
{
	if (!IsConnected())	return false;

	CriticalSectionSync sync(cs);
	recvedStream.ResetStream();

	if (recvedStream.GetFreeSize() == 0)
		return true;

	OverlappedRecvContext* recvContext = new OverlappedRecvContext(this);

	DWORD recvbytes{ 0 };
	DWORD flags{ 0 };
	recvContext->wsaBuf.len = static_cast<ULONG>(recvedStream.GetFreeSize());
	recvContext->wsaBuf.buf = recvedStream.GetStream();
	
	/// start real recv
	if (SOCKET_ERROR == WSARecv(sessionSocket, &recvContext->wsaBuf, 1, &recvbytes, &flags, reinterpret_cast<LPWSAOVERLAPPED>(recvContext), nullptr))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(recvContext);
			logManager->WriteLog("ClientSession::PostRecv Error : %d", GetLastError());
			return false;
		}
	}

	return true;
}

void ClientSession::RecvCompletion(DWORD transferred)
{
	CriticalSectionSync sync(cs);

	//Check Header
	Packet tempPacket;
	tempPacket.GetDataFromStream(recvedStream,transferred);
	PACKET_TYPE packetType = tempPacket.GetPacketType();
	
	//Analyze Packet
	Packet* packet{ nullptr };
	bool requestOK{ false };

	switch (packetType)
	{
	case PACKET_TYPE::LOGIN_SERVER:
	{
		LoginPacket* loginPacket = new LoginPacket();
		loginPacket->GetDataFromStream(recvedStream, transferred);
		loginPacket->GetTypeFromPacket();
		packet = dynamic_cast<Packet*>(loginPacket);
		break;
	}
	default:
		assert(false);
	}


	//Process Packet
	switch (packetType)
	{
	case PACKET_TYPE::LOGIN_SERVER:
		switch (dynamic_cast<LoginPacket*>(packet)->GetLoginPacketType())
		{
		case LOGIN_PACKET_TYPE::PROCESS_START:
			requestOK = dynamic_cast<LoginPacket*>(packet)->SetCoreConnection("core");
			break;
		default:
			assert(false);
		}

		break;
	case PACKET_TYPE::DATABASE_SERVER:
		break;
	default:
		assert(false);
	}


	//loginPacket.SetCoreConnection("core");



	packet->SetDataToStream(sendStream);

	delete packet;
}

bool ClientSession::PostSend()
{
	CriticalSectionSync sync(cs);
	
	if (!IsConnected())
		return false;

	if (sendStream.GetStreamLength() == 0)
		return true;

	OverlappedSendContext* sendContext = new OverlappedSendContext(this);

	DWORD sendbytes{ 0 };
	DWORD flags{ 0 };
	sendContext->wsaBuf.len = static_cast<ULONG>(sendStream.GetStreamLength());
	sendContext->wsaBuf.buf = sendStream.GetStream();

	/// start async send
	if (SOCKET_ERROR == WSASend(sessionSocket, &sendContext->wsaBuf, 1, &sendbytes, flags, (LPWSAOVERLAPPED)sendContext, nullptr))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			DeleteIoContext(sendContext);
			logManager->WriteLog("ClientSession::PostSend Error : %d", GetLastError());

			return false;
		}
	}

	return true;
}

void ClientSession::SendCompletion(DWORD transferred)
{
	CriticalSectionSync sync(cs);

	sendStream.RemoveSendedLength(transferred);
}


void ClientSession::AddRef()
{
	CriticalSectionSync sync(cs);

#ifdef _DEBUG
	assert(InterlockedIncrement(&refCount) > 0);
#else
	if (InterlockedIncrement(&refCount) < 0)
		exit(-1);
#endif
}

void ClientSession::ReleaseRef()
{
	CriticalSectionSync sync(cs);

	long ret = InterlockedDecrement(&refCount);
#ifdef _DEBUG
	assert(ret >= 0);
#else
	if (ret < 0)
		exit(-1);
#endif
	if (ret == 0)
		sessionManager->ReturnClientSession(this);
}


void DeleteIoContext(OverlappedIOContext* context)
