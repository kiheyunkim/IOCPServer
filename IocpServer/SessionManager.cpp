#include "stdafx.h"
#include "Protocol.h"
#include "NetworkSession.h"
#include "SessionManager.h"
#include "IocpManager.h"


SessionManager* sessionManager = nullptr;

SessionManager::SessionManager() : currentIssueCount(0), currentReturnCount(0), freeSessionList(MAX_CONNECTION) {}

SessionManager::~SessionManager()
{
	while (!freeSessionList.isEmpty())
	{
		delete freeSessionList.GetFront();
		freeSessionList.PopFront();
	}

	for (auto it : sessionList)
		delete it.second;
}

void SessionManager::PrepareSessions()
{
	CriticalSectionSync sync(cs);

	for (int i = 0; i < MAX_CONNECTION; ++i)
	{
		NetworkSession* client = new NetworkSession();
		freeSessionList.PushBack(client);
	}

	sessionsReady = true;
 }

void SessionManager::ReturnClientSession(NetworkSession* client)
{
	CriticalSectionSync sync(cs);

	assert(client->connected == 0 && client->refCount == 0);
	client->ResetSession();
	freeSessionList.PushBack(client);
	++currentReturnCount;
}

bool SessionManager::AcceptSessions()
{
	CriticalSectionSync sync(cs);

	while (currentIssueCount - currentReturnCount < MAX_CONNECTION)
	{
		NetworkSession* newClient = *freeSessionList.GetFront();

		if (newClient->IsConnected()) 
			return false;

		freeSessionList.PopFront();
		++currentIssueCount;
		newClient->AddRef();

		sessionList.insert(std::make_pair(newClient->GetSocket(), newClient));

		if (false == newClient->PostAccept())
			return false;
	}

	return true;
}

bool SessionManager::ConnectSession(const char* ipAddr, unsigned int port, SessionType type)
{
	CriticalSectionSync sync(cs);

	NetworkSession* newClient = *freeSessionList.GetFront();

	if (newClient->IsConnected())
		return false;

	newClient->ConvertToConnectionSession();

	freeSessionList.PopFront();
	++currentIssueCount;
	newClient->AddRef();

	if (false == newClient->Connect(ipAddr, port, type))
		return false;

	return true;
}

NetworkSession* SessionManager::GetSessionBySocket(SOCKET clientSocket)
{
	CriticalSectionSync sync(cs);

	return sessionList.find(clientSocket) != sessionList.end() ? sessionList.find(clientSocket)->second : nullptr;
}