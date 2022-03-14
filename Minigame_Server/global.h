

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
		USER_LOGIN,
		USER_LOGOUT,

		LOBBY_CHAT,

		USER_STARTMATCHING,
		USER_STOPMATCHING,
	};
}

namespace MATCH
{
	enum class TASK_TYPE
	{
		
	};
}

namespace PACKETINFO
{
	enum class SERVER_TO_CLIENT : unsigned char
	{
		LOGINOK,
		LOGINFAIL,
		ADDPLAYER,
		REMOVEPLAYER,
		LOBBYCHAT,

	};

	enum class CLIENT_TO_SERVER : unsigned char
	{
		LOGIN,
		LOBBYCHAT,
		STARTMATCHING,
		STOPMATCHING,
	};
}

#pragma pack(push, 1)
namespace PACKET
{
	namespace SERVER_TO_CLIENT
	{
		struct LoginOkPacket
		{
			unsigned char size = sizeof( LoginOkPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::LOGINOK;
		};

		struct LoginFailPacket
		{
			unsigned char size = sizeof( LoginFailPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::LOGINFAIL;
		};

		struct AddPlayerPacket
		{
			unsigned char size = sizeof( AddPlayerPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::ADDPLAYER;
			wchar_t nickname[ 10 ];
		};

		struct RemovePlayerPacket
		{
			unsigned char size = sizeof( RemovePlayerPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::REMOVEPLAYER;
			wchar_t nickname[ 10 ];
		};

		struct LobbyChatPacket
		{
			unsigned char size = sizeof( LobbyChatPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::LOBBYCHAT;
			wchar_t nickname[ 10 ];
			wchar_t message[ 50 ];
		};
	}

	namespace CLIENT_TO_SERVER
	{
		struct LoginPacket
		{
			unsigned char size = sizeof( LoginPacket );
			PACKETINFO::CLIENT_TO_SERVER type = PACKETINFO::CLIENT_TO_SERVER::LOGIN;
			wchar_t nickname[ 10 ];
		};

		struct LobbyChatPacket
		{
			unsigned char size = sizeof( LobbyChatPacket );
			PACKETINFO::CLIENT_TO_SERVER type = PACKETINFO::CLIENT_TO_SERVER::LOBBYCHAT;
			wchar_t message[ 50 ];
		};
	}
}
#pragma pack()
