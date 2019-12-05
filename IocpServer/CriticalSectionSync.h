#pragma once
#include"header.h"

class CriticalSection
{
private:
	CRITICAL_SECTION cs;

public:
	CriticalSection() 
	{
		InitializeCriticalSection(&cs); 
	}

	~CriticalSection() 
	{
		DeleteCriticalSection(&cs); 
	}

	inline void Enter()
	{
		EnterCriticalSection(&cs); 
	}

	inline void Leave()
	{
		LeaveCriticalSection(&cs); 
	}
};

class CriticalSectionSync
{
private:
	CriticalSection cs;

public:
	CriticalSectionSync()
	{
		cs.Enter();
	}

	~CriticalSectionSync()
	{
		cs.Leave();
	}
};
