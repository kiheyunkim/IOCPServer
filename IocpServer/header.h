#pragma once

#pragma comment(lib,"ws2_32")
#pragma comment(lib,"mswsock.lib")

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define MAX_CONNECTION 2000


#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>

#include <iostream>
#include <process.h>

//STL�� Thread-Safe���� ������ �ݵ�� �����ؾ���
#include<map>
#include<list>