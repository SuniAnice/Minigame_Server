

#pragma once

#include <WS2tcpip.h>
#include <MSWSock.h>

#include <iostream>


#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")


constexpr int PORT = 4000;
constexpr int BUFFER_SIZE = 1024;

enum class OP_TYPE
{
	OP_RECV, OP_SEND, OP_ACCEPT
};

enum class USER_STATE
{
	STATE_READY, STATE_CONNECTED, STATE_INGAME
};

struct OVERLAPPED_EXTENDED
{
	WSAOVERLAPPED overlapped;
	WSABUF wsaBuf;

	unsigned char packetBuffer[ BUFFER_SIZE ];
	OP_TYPE opType;
	SOCKET socket;
};

struct Session {
	OVERLAPPED_EXTENDED overlapped;
	SOCKET socket;
	USER_STATE state = USER_STATE::STATE_READY;
	int key;
	int prevSize;
};

namespace LOBBY
{
	enum class TASK_TYPE 
	{
		USER_ACCEPT,
	};

	struct AcceptInfo
	{
		SOCKET socket;
	};
}
