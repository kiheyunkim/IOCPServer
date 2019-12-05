#ifndef _IOCPMANAGER_H_
#define _IOCPMANAGER_H_

class NetworkSession;
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
	std::vector<HANDLE>			threadsHandles;

private:
	std::size_t connectCount;
	std::size_t standbyCount;

private:
	static unsigned int WINAPI IoWorkerThread(LPVOID lpParam);
	static bool acceptCompletion(NetworkSession* session, OverlappedAcceptContext* context);
	static bool connectCompletion(NetworkSession* session, OverlappedConnectContext* context);
	static bool receiveCompletion(NetworkSession* session, OverlappedRecvContext* context, DWORD transferred);
	static bool sendCompletion(NetworkSession* session, OverlappedSendContext* context, DWORD transferred);
	static bool disconnectCompletion(NetworkSession* client, DisconnectReason dr);

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
	bool ConnectDBServer(const char* ipAddr, unsigned int port);
	bool COnnectLoginServer(const char* ipAddr, unsigned int port);
	bool StartAccept();
	void CleanupIocp();

public:
	inline HANDLE GetComletionPort()	{ return completionPortHandle; }
	inline int	GetIoThreadCount()		{ return ioThreadCount; }
	inline SOCKET* GetStandbySocket()	{ return &standbySocket; }
	GameRoomManager* GetGameRoomMgr()	{ return gameRoomMgr; }

public:
	int GetMaxConnection() { return MAX_CONNECTION; }
};

extern IocpManager* iocpManager;

BOOL DisconnectEx(SOCKET sock, LPOVERLAPPED overlapped, DWORD flags, DWORD reserved);
BOOL AcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, PVOID lpOutputBuffer, DWORD dwReceiveDataLength, 
	DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped);
BOOL ConnectEx(SOCKET hSocket, const struct sockaddr* name, int namelen, PVOID lpSendBuffer,
	DWORD dwSendDataLength, LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped);

#endif // !_IOCPMANAGER_H_
