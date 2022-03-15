

#include "GameManager.h"
#include "MainServer.h"


GameManager::GameManager()
{
}

GameManager::~GameManager()
{
	for ( auto& room : m_rooms )
	{
		delete room;
	}
}

void GameManager::ThreadFunc()
{
	std::pair <INGAME::TASK_TYPE, void* > task;
	while ( true )
	{
		if ( !m_tasks.try_pop( task ) )
		{
			std::this_thread::yield();
			continue;
		}
		switch ( task.first )
		{
		case INGAME::TASK_TYPE::ROOM_CREATE:
		{
			INGAME::CreateRoomTask* t = reinterpret_cast< INGAME::CreateRoomTask* >( task.second );
			if ( t != nullptr )
			{
				m_rooms.emplace_back( t->room );
				PACKET::SERVER_TO_CLIENT::GameMatchedPacket packet;
				for ( int i = 0; i < MAX_PLAYER_IN_ROOM; i++ )
				{
					packet.users[ i ].userNum = t->room->userSessions[ i ]->key;
					wmemcpy( packet.users[ i ].nickname, t->room->userSessions[ i ]->nickname.c_str(), t->room->userSessions[ i ]->nickname.size() );
				}
				for ( auto& pl : t->room->userSessions )
				{
					MainServer::GetInstance().SendPacket( pl->socket, &packet );
				}
				delete task.second;
			}
		}
			break;
		}
	}
}
