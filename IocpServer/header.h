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

//STL은 Thread-Safe이지 않으니 반드시 수정해야함
#include<map>
#include<list>