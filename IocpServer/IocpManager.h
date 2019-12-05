#ifndef _IOCPMANAGER_H_
#define _IOCPMANAGER_H_

#include "header.h"

class ClientSession;
enum class DisconnectReason;

struct OverlappedAcceptContext;
struct OverlappedConnectContext;
struct OverlappedSendContext;
struct OverlappedRecvContext;
struct OverlappedDisconnectContext;
struct OverlappedAcceptContext;

class IocpManager
{
private:
	HANDLE						completionPortHandle;
	int							ioThreadCount;
	SOCKET						standbySocket;
	HANDLE*						threadHandles;

private:
	static unsigned int WINAPI IoWorkerThread(LPVOID lpParam);
	static bool acceptCompletion(ClientSession* session, OverlappedAcceptContext* context);
	static bool receiveCompletion(ClientSession* session, OverlappedRecvContext* context, DWORD transferred);
	static bool sendCompletion(ClientSession* session, OverlappedSendContext* context, DWORD transferred);
	static bool disconnectCompletion(ClientSession* client, DisconnectReason dr);

public:
	static char					acceptBuffer[64];
	static LPFN_ACCEPTEX		fnAcceptEx;
	static LPFN_CONNECTEX		fnConnectEx;
	static LPFN_DISCONNECTEX	fnDisconnectEx;

public:
	IocpManager();
	~IocpManager();

	bool InitializeIocp(unsigned short port);
	bool StartIoThreads();
	bool StartAccept();
	void CleanupIocp();

public:
	inline HANDLE GetComletionPort()	{ return completionPortHandle; }
	inline int	GetIoThreadCount()		{ return ioThreadCount; }
	inline SOCKET* GetStandbySocket()	{ return &standbySocket; }
};

extern IocpManager* iocpManager;

BOOL DisconnectEx(SOCKET sock, LPOVERLAPPED overlapped, DWORD flags, DWORD reserved);
BOOL AcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, PVOID lpOutputBuffer, DWORD dwReceiveDataLength, 
	DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped);
BOOL ConnectEx(SOCKET hSocket, const struct sockaddr* name, int namelen, PVOID lpSendBuffer,
	DWORD dwSendDataLength, LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped);

#endif // !_IOCPMANAGER_H_
