#include "ClientSession.h"
#include "IocpManager.h"
#include "CriticalSectionSync.h"
#include "SessionManager.h"


SessionManager* sessionManager = nullptr;

SessionManager::SessionManager() : currentIssueCount(0), currentReturnCount(0) {}

SessionManager::~SessionManager()
{
	while (!freeSessionList.empty())
	{
		delete freeSessionList.back();
		freeSessionList.pop_back();
	}

	for (auto it : sessionList)
		delete it.second;
}

void SessionManager::PrepareSessions()
{
	CriticalSectionSync sync;

	for (int i = 0; i < MAX_CONNECTION; ++i)
	{
		ClientSession* client = new ClientSession();
		freeSessionList.push_back(client);
	}
 }

void SessionManager::ReturnClientSession(ClientSession* client)
{
	CriticalSectionSync sync;

	//assert(client->connected == 0 && client->refCount == 0);
	client->ResetSession();
	freeSessionList.push_back(client);
	++currentReturnCount;
}

bool SessionManager::AcceptSessions()
{
	CriticalSectionSync sync;

	while (currentIssueCount - currentReturnCount < MAX_CONNECTION)
	{
		ClientSession* newClient = freeSessionList.front();

		if (newClient->IsConnected()) 
			return false;

		freeSessionList.pop_front();
		++currentIssueCount;
		newClient->AddRef();

		sessionList.insert(std::make_pair(newClient->GetSocket(), newClient));

		if (false == newClient->PostAccept())
			return false;
	}

	return true;
}