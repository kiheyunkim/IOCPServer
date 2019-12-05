#include"IocpManager.h"
#include "SessionManager.h"

int main(int argc, char* argv[])
{
	iocpManager = new IocpManager();
	sessionManager = new SessionManager();

	if (!iocpManager->InitializeIocp(9900))
		return -1;

	if (!iocpManager->StartIoThreads())
		return -1;

	if (!iocpManager->StartAccept())
		return -1;

	iocpManager->CleanupIocp();

	delete iocpManager;
	delete sessionManager;
	
	return 0;
}