

#pragma once

#include <WS2tcpip.h>
#include <MSWSock.h>

#include <chrono>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <chrono>


#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")


using namespace std::chrono;


constexpr int PORT = 4000;
constexpr int BUFFER_SIZE = 1024;
constexpr int MAX_PLAYER_IN_ROOM = 4;
constexpr int MAX_ROUND = 5;
constexpr size_t MAX_USER = 100000;
constexpr seconds WAIT_TIME = 10s;
constexpr seconds READY_TIME = 30s;
constexpr seconds GAME_TIME = 180s;


enum class OP_TYPE
{
	OP_RECV, OP_SEND, OP_ACCEPT
};

enum class GAME_STATE
{
	ROUND_WAIT,
	ROUND_READY,
	ROUND_START,
	GAME_END,
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
	int key;
	int prevSize;
	std::wstring nickname;
};

// 인게임 유저 정보
struct UserInfo
{
	int userNum;
	wchar_t nickname[ 10 ] = {};
};

struct GameRoom
{
	std::vector< Session* > userSessions;
	GAME_STATE state = GAME_STATE::ROUND_WAIT;
	std::unordered_map <int, UserInfo> userInfo;
	int currentRound = 1;
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

	struct LoginTask
	{
		int id;
		std::wstring nickname;
	};

	struct ChatTask
	{
		int id;
		std::wstring message;
	};
}

namespace MATCH
{
	enum class TASK_TYPE
	{
		USER_STARTMATCHING,
		USER_STOPMATCHING,
	};

	struct StartMatchingTask
	{
		Session* session;
	};

	struct StopMatchingTask
	{
		Session* session;
	};
}

namespace INGAME
{
	enum class TASK_TYPE
	{
		ROOM_CREATE,
		ROOM_REMOVE,
		ROUND_WAIT,
		ROUND_READY,
		ROUND_END,
	};

	struct CreateRoomTask
	{
		GameRoom* room;
	};

	struct RoundWaitTask
	{
		GameRoom* room;
	};

	struct RoundReadyTask
	{
		GameRoom* room;
		int currentRound;
	};

	struct RoundEndTask
	{
		GameRoom* room;
		int currentRound;
	};

	struct RemoveRoomTask
	{
		GameRoom* room;
	};
}

struct TimerEvent
{
	std::chrono::system_clock::time_point time;
	std::pair < INGAME::TASK_TYPE, void* > task;

	bool operator< ( const TimerEvent& e ) const
	{
		return this->time > e.time;
	}
};

namespace PACKETINFO
{
	enum class SERVER_TO_CLIENT : unsigned char
	{
		LOGINOK,
		LOGINFAIL,
		ADDPLAYER,
		REMOVEPLAYER,
		LOBBYCHAT,
		GAMEMATCHED,
		ROUNDREADY,
		ROUNDSTART,
	};

	enum class CLIENT_TO_SERVER : unsigned char
	{
		LOGIN,
		LOBBYCHAT,
		STARTMATCHING,
		STOPMATCHING,
		GAMEREADY,
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
			int index;
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
			wchar_t nickname[ 10 ] = {};
		};

		struct RemovePlayerPacket
		{
			unsigned char size = sizeof( RemovePlayerPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::REMOVEPLAYER;
			wchar_t nickname[ 10 ] = {};
		};

		struct LobbyChatPacket
		{
			unsigned char size = sizeof( LobbyChatPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::LOBBYCHAT;
			wchar_t nickname[ 10 ] = {};
			wchar_t message[ 50 ] = {};
		};

		struct GameMatchedPacket
		{
			unsigned char size = sizeof( GameMatchedPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::GAMEMATCHED;
			UserInfo users[ MAX_PLAYER_IN_ROOM ];
		};

		struct RoundReadyPacket
		{
			unsigned char size = sizeof( RoundReadyPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::ROUNDREADY;
		};

		struct RoundStartPacket
		{
			unsigned char size = sizeof( RoundStartPacket );
			PACKETINFO::SERVER_TO_CLIENT type = PACKETINFO::SERVER_TO_CLIENT::ROUNDSTART;
		};
	}

	namespace CLIENT_TO_SERVER
	{
		struct LoginPacket
		{
			unsigned char size = sizeof( LoginPacket );
			PACKETINFO::CLIENT_TO_SERVER type = PACKETINFO::CLIENT_TO_SERVER::LOGIN;
			wchar_t nickname[ 10 ] = {};
		};

		struct LobbyChatPacket
		{
			unsigned char size = sizeof( LobbyChatPacket );
			PACKETINFO::CLIENT_TO_SERVER type = PACKETINFO::CLIENT_TO_SERVER::LOBBYCHAT;
			wchar_t message[ 50 ] = {};
		};

		struct StartMatchingPacket
		{
			unsigned char size = sizeof( StartMatchingPacket );
			PACKETINFO::CLIENT_TO_SERVER type = PACKETINFO::CLIENT_TO_SERVER::STARTMATCHING;
		};

		struct StopMatchingPacket
		{
			unsigned char size = sizeof( StopMatchingPacket );
			PACKETINFO::CLIENT_TO_SERVER type = PACKETINFO::CLIENT_TO_SERVER::STOPMATCHING;
		};

		struct GameReadyPacket
		{
			unsigned char size = sizeof( GameReadyPacket );
			PACKETINFO::CLIENT_TO_SERVER type = PACKETINFO::CLIENT_TO_SERVER::GAMEREADY;
		};
	}
}
#pragma pack(pop)
