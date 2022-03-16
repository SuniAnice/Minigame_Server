

#include "GameManager.h"
#include "MainServer.h"
#include "LobbyManager.h"
#include "TimerManager.h"


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
			std::this_thread::sleep_for( 10ms );
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
				BroadCastPacket( t->room, &packet );

				// �κ񿡼� �÷��̾� ����
				for ( auto& pl : t->room->userSessions )
				{
					LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::USER_EXITLOBBY, new LOBBY::ExitLobbyTask{ pl } );
				}

				TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + WAIT_TIME, INGAME::TASK_TYPE::ROUND_WAIT, new INGAME::RoundWaitTask{ t->room } );
				delete task.second;
			}
		}
			break;
		case INGAME::TASK_TYPE::ROUND_WAIT:
		{
			INGAME::RoundWaitTask* t = reinterpret_cast<INGAME::RoundWaitTask*>( task.second );
			if ( t != nullptr )
			{
				PACKET::SERVER_TO_CLIENT::RoundReadyPacket packet;

				packet.seeker = t->room->userSessions[ PickSeeker( t->room ) ]->key;

				// �����鿡�� ���� �غ� �˸�
				BroadCastPacket( t->room, &packet );
				TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + READY_TIME, INGAME::TASK_TYPE::ROUND_READY, new INGAME::RoundReadyTask{ t->room, t->room->currentRound } );
				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::ROUND_READY:
		{
			INGAME::RoundReadyTask* t = reinterpret_cast<INGAME::RoundReadyTask*>( task.second );
			if ( t != nullptr )
			{
				// �ٸ� ������ ���尡 ������� �ʾҴٸ�
				if ( t->currentRound == t->room->currentRound )
				{
					PACKET::SERVER_TO_CLIENT::RoundStartPacket packet;

					// �����鿡�� ���� ������ �˸�
					BroadCastPacket( t->room, &packet );

					TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + GAME_TIME, INGAME::TASK_TYPE::ROUND_END, new INGAME::RoundEndTask{ t->room, t->room->currentRound } );
				}
				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::ROUND_END:
		{
			INGAME::RoundEndTask* t = reinterpret_cast<INGAME::RoundEndTask*>( task.second );
			if ( t != nullptr )
			{
				// �ٸ� ������ ���尡 ������� �ʾҴٸ�
				if ( t->currentRound == t->room->currentRound )
				{
					// ������ ���尡 ������ �ʾҴٸ�
					if ( t->room->currentRound < MAX_ROUND )
					{
						t->room->currentRound++;

						PACKET::SERVER_TO_CLIENT::RoundReadyPacket packet;

						packet.seeker = t->room->userSessions[ PickSeeker( t->room ) ]->key;

						// �����鿡�� ���� �غ� �˸�
						BroadCastPacket( t->room, &packet );

						TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + READY_TIME, INGAME::TASK_TYPE::ROUND_READY, new INGAME::RoundReadyTask{ t->room } );
					}
					else
					{
						t->room->currentRound++;

						// �κ� �÷��̾� ���
						for ( auto& pl : t->room->userSessions )
						{
							LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::USER_ENTERLOBBY, new LOBBY::EnterLobbyTask{ pl } );
						}

						// ���� ���� ó�� �� �� ���� �½�ũ ���
						TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + GAME_TIME, INGAME::TASK_TYPE::ROOM_REMOVE, new INGAME::RemoveRoomTask{ t->room } );
					}
					
				}
				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::ROOM_REMOVE:
		{
			INGAME::RemoveRoomTask* t = reinterpret_cast<INGAME::RemoveRoomTask*>( task.second );
			if ( t != nullptr )
			{
				m_rooms.erase( std::remove( m_rooms.begin(), m_rooms.end(), t->room ) );
				delete (t->room);
				
				delete task.second;
			}
		}
		break;
		}
	}
}

void GameManager::BroadCastPacket( GameRoom* room, void* packet )
{
	for ( auto& pl : room->userSessions )
	{
		MainServer::GetInstance().SendPacket( pl->socket, packet );
	}
}

int GameManager::PickSeeker( GameRoom* room )
{
	int temp = rand() % MAX_PLAYER_IN_ROOM + 1;
	// ���� ����� ������ ���� �ʵ���
	while ( temp == room->currentSeeker )
	{
		temp = rand() % MAX_PLAYER_IN_ROOM + 1;
	}

	return temp;
}
