

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


constexpr int		PORT =					4000;
constexpr int		BUFFER_SIZE =			1024;
constexpr int		MAX_PLAYER_IN_ROOM =	4;		// 최대 방 인원수
constexpr int		SEEKER_COUNT =			1;		// 술래 인원수
constexpr int		MAX_ROUND =				5;		// 최대 진행 라운드
constexpr int		NUM_OF_OBJECTS =		25;		// 랜덤하게 설정될 오브젝트의 수
constexpr size_t	MAX_USER =				100000;	// 최대 동접자
constexpr seconds	WAIT_TIME =				5s;		// 라운드 대기 시간
constexpr seconds	READY_TIME =			15s;	// 라운드 준비 시간
constexpr seconds	GAME_TIME =				180s;	// 라운드 진행 시간
constexpr seconds	INTERVAL_TIME =			5s;		// 라운드 종료 인터벌
constexpr double	ATTACK_RANGE =			300;	// 공격 사정거리
constexpr double	ATTACK_ANGLE =			45;		// 공격 각도
constexpr int		SCORE_SEEKERWIN =		100;	// 술래팀 기본 승리 보상
constexpr int		SCORE_SEEKERKILL =		200;	// 술래의 찾은 유저 보상
constexpr int		SCORE_SEEKERTIME =		3;		// 술래의 승리시 남은 시간 보상
constexpr int		SCORE_HIDERWIN =		50;		// 숨는팀 기본 승리 보상
constexpr int		SCORE_HIDERSURVIVE =	300;	// 숨는팀 최후 생존 보상
constexpr int		SCORE_HIDERTIME =		2;		// 숨는팀 생존 시간 보상
constexpr int		EVENT_COUNT =			3;		// 랜덤하게 발생할 수 있는 이벤트의 갯수
constexpr seconds	EVENT_1_INTERVAL =		30s;	// 라운드 시작 후 첫번째 이벤트 사이의 시간
constexpr seconds	EVENT_2_INTERVAL =		60s;	// 첫번째 이벤트와 두번째 이벤트의 간격
constexpr seconds	EVENT_3_INTERVAL =		40s;	// 두번째 이벤트와 세번째 이벤트의 간격


enum class EOpType
{
	Recv, Send, Accept
};

struct OverlappedExtended
{
	WSAOVERLAPPED m_overlapped;
	WSABUF m_wsaBuf;

	unsigned char m_packetBuffer[ BUFFER_SIZE ] = {};
	EOpType m_opType;
	SOCKET m_socket;
};

struct Session {
	OverlappedExtended m_overlapped;
	SOCKET m_socket;
	int m_key = 0;
	int m_prevSize = 0;
	std::wstring m_nickname;
	int m_roomIndex = -1;
	bool m_isMatching = false;
	int m_totalScore = 0;
};

// 인게임 유저 정보
struct UserInfo
{
	int m_userNum = 0;
	wchar_t m_nickname[ 10 ] = {};
	bool m_isAlive = true;
	float m_x = 0;
	float m_y = 0;
	float m_z = 0;
	float m_angle = 0;
	int m_score = 0;
	int m_object = 0;
};

struct GameRoom
{
	std::vector< Session* > m_userSessions;
	std::unordered_map <int, UserInfo> m_userInfo;
	unsigned int m_roomNum = 0;
	int m_currentRound = 1;
	int m_currentSeeker = -1;
	int m_aliveHider = -1;
	std::chrono::steady_clock::time_point m_roundStart;
	bool m_gameEnded = false;
};


namespace Lobby
{
	enum class ETaskType 
	{
		UserAccept,
		UserLogin,
		UserLogout,

		LobbyChat,

		StartMatching,
		StopMatching,

		EnterLobby,
		ExitLobby,
		OfferLobbyInfo,
		DBInfoLoaded,
	};

	struct LoginTask
	{
		int m_id;
		std::wstring m_nickname;
	};

	struct ChatTask
	{
		int m_id;
		std::wstring m_message;
	};

	struct EnterLobbyTask
	{
		Session* m_session;
		int m_score;
	};

	struct ExitLobbyTask
	{
		Session* m_session;
		size_t m_roomNum;
	};

	struct OfferLobbyInfoTask
	{
		Session* m_session;
	};

	struct DBInfoLoadedTask
	{
		Session* m_session;
		std::wstring m_nickname;
		int m_score;
	};

}

namespace Match
{
	enum class ETaskType
	{
		StartMatching,
		StopMatching,
		UserRemove,
	};

	struct StartMatchingTask
	{
		Session* m_session;
	};

	struct StopMatchingTask
	{
		Session* m_session;
	};

	struct RemovePlayerTask
	{
		Session* m_session;
	};
}

namespace INGAME
{
	enum class ETaskType
	{
		RoomCreate,
		RoomRemove,
		RoundWait,
		RoundReady,
		RoundEnd,
		MovePlayer,
		AttackPlayer,
		RemovePlayer,
		RoundResult,
		ProcessEvent,
		CheckAlive,
	};

	struct CreateRoomTask
	{
		GameRoom* m_room;
	};

	struct RoundWaitTask
	{
		GameRoom* m_room;
	};

	struct RoundReadyTask
	{
		GameRoom* m_room;
		int m_currentRound;
	};

	struct RoundEndTask
	{
		GameRoom* m_room;
		int m_currentRound;
	};

	struct RemoveRoomTask
	{
		GameRoom* m_room;
	};

	struct MovePlayerTask
	{
		Session* m_session;
		int m_index;
		float m_x;
		float m_y;
		float m_z;
		float m_angle;
	};

	struct AttackPlayerTask
	{
		Session* m_session;
		int m_index;
	};

	struct RemovePlayerTask
	{
		int m_roomindex;
		int m_index;
		Session* m_session;
	};

	struct RoundResultTask
	{
		GameRoom* m_room;
		int m_currentRound;
		bool m_isSeekerWin;
	};

	struct ProcessEventTask
	{
		GameRoom* m_room;
		int m_currentRound;
		int m_eventcount = 0;
	};

	struct CheckAliveTask
	{
		Session* m_session;
	};
}

namespace DB
{
	enum class ETaskType
	{
		LoadInfo,
		SaveInfo,
	};

	struct LoadTask
	{
		Session* m_Session;
		std::wstring m_nickname;
	};

	struct SaveTask
	{
		Session* m_Session;
		int m_score;
	};
}

struct TimerEvent
{
	std::chrono::steady_clock::time_point m_time;
	std::pair < INGAME::ETaskType, void* > m_task;

	bool operator< ( const TimerEvent& e ) const
	{
		return this->m_time > e.m_time;
	}
};

namespace PacketInfo
{
	enum class EServerToClient : unsigned char
	{
		LoginOk,
		LoginFail,
		AddPlayer,
		RemovePlayer,
		LobbyChat,
		GameMatched,
		RoundReady,
		RoundStart,
		MovePlayer,
		Attack,
		RemovePlayerInGame,
		KillPlayer,
		GameEnd,
		SetPosition,
		GameResult,
		RandomEvent,
		ChangeObject,
		CheckAlive,
	};

	enum class EClientToServer : unsigned char
	{
		Login,
		LobbyChat,
		StartMatching,
		StopMatching,
		MovePlayer,
		Attack,
		MoveToLobby,
	};
}

#pragma pack(push, 1)
namespace Packet
{
	namespace ServerToClient
	{
		struct LoginOkPacket
		{
			unsigned char m_size = sizeof( LoginOkPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::LoginOk;
			int m_index;
		};

		struct LoginFailPacket
		{
			unsigned char m_size = sizeof( LoginFailPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::LoginFail;
		};

		struct AddPlayerPacket
		{
			unsigned char sm_sizeize = sizeof( AddPlayerPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::AddPlayer;
			wchar_t m_nickname[ 10 ] = {};
		};

		struct RemovePlayerPacket
		{
			unsigned char m_size = sizeof( RemovePlayerPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::RemovePlayer;
			wchar_t m_nickname[ 10 ] = {};
		};

		struct LobbyChatPacket
		{
			unsigned char m_size = sizeof( LobbyChatPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::LobbyChat;
			wchar_t m_nickname[ 10 ] = {};
			wchar_t m_message[ 50 ] = {};
		};

		struct GameMatchedPacket
		{
			unsigned char m_size = sizeof( GameMatchedPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::GameMatched;
			UserInfo m_users[ MAX_PLAYER_IN_ROOM ];
		};

		struct RoundReadyPacket
		{
			unsigned char m_size = sizeof( RoundReadyPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::RoundReady;
			int m_seeker = 0;
			int m_hiderNum[ MAX_PLAYER_IN_ROOM - SEEKER_COUNT ] = {};
			int m_object[ MAX_PLAYER_IN_ROOM - SEEKER_COUNT ] = {};
		};

		struct RoundStartPacket
		{
			unsigned char m_size = sizeof( RoundStartPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::RoundStart;
		};

		struct MovePlayerPacket
		{
			unsigned char m_size = sizeof( MovePlayerPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::MovePlayer;
			int m_index = 0;
			float m_x = 0;
			float m_y = 0;
			float m_z = 0;
			float m_angle = 0;
		};

		struct AttackPlayerPacket
		{
			unsigned char m_size = sizeof( AttackPlayerPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::Attack;
			int m_index = 0;
		};

		struct RemovePlayerIngamePacket
		{
			unsigned char m_size = sizeof( RemovePlayerIngamePacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::RemovePlayerInGame;
			int m_index = 0;
		};

		struct KillPlayerPacket
		{
			unsigned char m_size = sizeof( KillPlayerPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::KillPlayer;
			int m_killer = 0;
			int m_victim = 0;
		};

		struct GameEndPacket
		{
			unsigned char m_size = sizeof( GameEndPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::GameEnd;
			int m_userNum[ MAX_PLAYER_IN_ROOM ] = {};
			int m_userScore[ MAX_PLAYER_IN_ROOM ] = {};
		};

		struct SetPositionPacket
		{
			unsigned char m_size = sizeof( SetPositionPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::SetPosition;
			float m_x = 0;
			float m_y = 0;
			float m_z = 0;
		};

		struct GameResultPacket
		{
			unsigned char m_size = sizeof( GameResultPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::GameResult;
			bool m_isSeekerWin = false;
			int m_firstUser = 0;
		};

		struct RandomEventPacket
		{
			unsigned char m_size = sizeof( RandomEventPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::RandomEvent;
			int m_eventIndex = 0;
		};

		struct ChangeObjectPacket
		{
			unsigned char m_size = sizeof( ChangeObjectPacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::ChangeObject;
			int m_hiderNum[ MAX_PLAYER_IN_ROOM - SEEKER_COUNT ] = {};
			int m_object[ MAX_PLAYER_IN_ROOM - SEEKER_COUNT ] = {};
		};

		struct CheckAlivePacket
		{
			unsigned char m_size = sizeof( CheckAlivePacket );
			PacketInfo::EServerToClient m_type = PacketInfo::EServerToClient::CheckAlive;
		};
	}

	namespace ClientToServer
	{
		struct LoginPacket
		{
			unsigned char m_size = sizeof( LoginPacket );
			PacketInfo::EClientToServer m_type = PacketInfo::EClientToServer::Login;
			wchar_t m_nickname[ 10 ] = {};
		};

		struct LobbyChatPacket
		{
			unsigned char m_size = sizeof( LobbyChatPacket );
			PacketInfo::EClientToServer m_type = PacketInfo::EClientToServer::LobbyChat;
			wchar_t m_message[ 50 ] = {};
		};

		struct StartMatchingPacket
		{
			unsigned char m_size = sizeof( StartMatchingPacket );
			PacketInfo::EClientToServer m_type = PacketInfo::EClientToServer::StartMatching;
		};

		struct StopMatchingPacket
		{
			unsigned char m_size = sizeof( StopMatchingPacket );
			PacketInfo::EClientToServer m_type = PacketInfo::EClientToServer::StopMatching;
		};

		struct PlayerMovePacket
		{
			unsigned char m_size = sizeof( PlayerMovePacket );
			PacketInfo::EClientToServer m_type = PacketInfo::EClientToServer::MovePlayer;
			float m_x = 0;
			float m_y = 0;
			float m_z = 0;
			float m_angle = 0;
		};

		struct PlayerAttackPacket
		{
			unsigned char m_size = sizeof( PlayerAttackPacket );
			PacketInfo::EClientToServer m_type = PacketInfo::EClientToServer::Attack;
		};

		struct MoveToLobbyPacket
		{
			unsigned char m_size = sizeof( MoveToLobbyPacket );
			PacketInfo::EClientToServer m_type = PacketInfo::EClientToServer::MoveToLobby;
		};
	}
}
#pragma pack(pop)
