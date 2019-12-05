#pragma once

#include "header.h"
#include "ClientSession.h"

enum class IOType
{
	IO_NONE,
	IO_SEND,
	IO_RECV,
	IO_RECV_ZERO,
	IO_ACCEPT,
	IO_DISCONNECT
};

enum class DisconnectReason
{
	DR_NONE,
	DR_ACTIVE,
	DR_ONCONNECT_ERROR,
	DR_IO_REQUEST_ERROR,
	DR_COMPLETION_ERROR,
};

struct OverlappedIOContext
{
	OVERLAPPED			overlapped;
	ClientSession*		sessionObject;
	IOType				ioType;
	WSABUF				wsaBuf;

	OverlappedIOContext(ClientSession* session, IOType ioType) :
		sessionObject(session),
		ioType(ioType)
	{
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		memset(&wsaBuf, 0, sizeof(WSABUF));
		sessionObject->AddRef();
	}
};

struct OverlappedAcceptContext : public OverlappedIOContext
{
	OverlappedAcceptContext(ClientSession* owner) :
		OverlappedIOContext(owner, IOType::IO_ACCEPT) {}
};

struct OverlappedSendContext : public OverlappedIOContext
{
	OverlappedSendContext(ClientSession* session) :
		OverlappedIOContext(session, IOType::IO_SEND) {}
};

struct OverlappedRecvContext : public OverlappedIOContext
{
	OverlappedRecvContext(ClientSession* session) :
		OverlappedIOContext(session, IOType::IO_RECV) {}
};

struct OverlappedDisconnectContext : public OverlappedIOContext
{
	DisconnectReason disconnectReason;

	OverlappedDisconnectContext(ClientSession* session, DisconnectReason dr) :
		OverlappedIOContext(session, IOType::IO_DISCONNECT),
		disconnectReason(dr) {}
};

inline void DeleteIoContext(OverlappedIOContext* context)
{
	if (nullptr == context)
		return;

	context->sessionObject->ReleaseRef();
	delete context;
}