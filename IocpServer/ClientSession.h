#ifndef _NETWORKSESSION_H_
#define _NETWORKSESSION_H_


class ClientSession;
class SessionManager;



class ClientSession
{
private:
	friend class SessionManager;

private:
	SOCKET			sessionSocket;
	SOCKADDR_IN		sessionAddress;

	volatile long	refCount;
	volatile long	connected;

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
	
	unsigned short GetPort();
};

#endif // !_NETWORKSESSION_H_





