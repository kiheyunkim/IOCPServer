#ifndef _SESSIONMANGER_H_
#define _SESSIONMANGER_H_

#include "CriticalSection.h"
#include "CircularBuffer.h"

enum class SessionType;
class NetworkSession;
class SessionManager
{
private:
	bool sessionsReady;

private:
	CriticalSection cs;

private:
	std::map<SOCKET, NetworkSession*> sessionList;
	CircularBuffer<NetworkSession*> freeSessionList;

private:
	uint64_t currentIssueCount;
	uint64_t currentReturnCount;

public:
	SessionManager();	
	~SessionManager();

public:
	void PrepareSessions();
	bool AcceptSessions();
	void ReturnClientSession(NetworkSession* client);
	NetworkSession* GetSessionBySocket(SOCKET clientSocket);

public:
	bool GetReadyState() { return sessionsReady; }

public:
	bool ConnectSession(const char* ipAddr, unsigned int port, SessionType type);
};

extern SessionManager* sessionManager;

#endif // !_SESSIONMANGER_H_