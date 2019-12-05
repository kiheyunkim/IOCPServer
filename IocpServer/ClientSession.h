#ifndef _NETWORKSESSION_H_
#define _NETWORKSESSION_H_

#include "header.h"

constexpr int bufferSize =  1024;

class ClientSession;
class SessionManager;
enum class DisconnectReason;

class ClientSession
{
private:
	friend class SessionManager;

private:
	SOCKET			sessionSocket;
	SOCKADDR_IN		sessionAddress;

	volatile long	refCount;
	volatile long	connected;

private:
	char sendBuffer[bufferSize];
	char recvBuffer[bufferSize];

public:
	ClientSession();
	~ClientSession();

	void	ResetSession();
	bool	IsConnected() const { return !!connected; }

	bool	PostAccept();
	void	AcceptCompletion();

	bool	PostRecv();
	void	RecvCompletion(DWORD transferred);

	bool	PostSend();
	void	SendCompletion(DWORD transferred);

	void	DisconnectRequest(DisconnectReason dr);
	void	DisconnectCompletion(DisconnectReason dr);

	void	AddRef();
	void	ReleaseRef();

	inline	void SetSocket(SOCKET sock) { sessionSocket = sock; }
	inline	SOCKET GetSocket() const { return sessionSocket; }
};

#endif // !_NETWORKSESSION_H_





