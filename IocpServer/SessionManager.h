#ifndef _SESSIONMANGER_H_
#define _SESSIONMANGER_H_

enum class SessionType;
class ClientSession;
class SessionManager
{
private:
	std::map<SOCKET, ClientSession*> sessionList;
	std::list<ClientSession*> freeSessionList;

private:
	uint64_t currentIssueCount;
	uint64_t currentReturnCount;

public:
	SessionManager();	
	~SessionManager();

public:
	void PrepareSessions();
	bool AcceptSessions();
	void ReturnClientSession(ClientSession* client);
};

extern SessionManager* sessionManager;

#endif // !_SESSIONMANGER_H_