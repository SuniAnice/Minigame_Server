

#include "GameManager.h"
#include "MainServer.h"
#include "LobbyManager.h"
#include "TimerManager.h"
#include "LogUtil.h"


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
					LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::USER_EXITLOBBY, new LOBBY::ExitLobbyTask{ pl, m_rooms.size() - 1 } );
				}

				TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + WAIT_TIME, INGAME::TASK_TYPE::ROUND_WAIT, new INGAME::RoundWaitTask{ t->room } );
				PRINT_LOG("�� ���� ��û ����");
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
				PRINT_LOG( "���� ���� - ���� �غ� ���·� ��ȯ" );
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
					PRINT_LOG( "���� ���� - ���� ���� ���·� ��ȯ" );
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

						TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + READY_TIME, INGAME::TASK_TYPE::ROUND_READY, new INGAME::RoundReadyTask{ t->room, t->room->currentRound } );
						PRINT_LOG( "���� ���� - ���� ���� ���۵�" );
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
						PRINT_LOG( "���� ���� - ����" );
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
				PRINT_LOG( "�� ���� ��û ����" );

				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::MOVE_PLAYER:
		{
			INGAME::MovePlayerTask* t = reinterpret_cast<INGAME::MovePlayerTask*>( task.second );
			if ( t != nullptr )
			{
				PACKET::SERVER_TO_CLIENT::MovePlayerPacket packet;
				packet.index = t->index;
				m_rooms[ t->session->roomIndex ]->userInfo[ t->index ].x = packet.x = t->x;
				m_rooms[ t->session->roomIndex ]->userInfo[ t->index ].y = packet.y = t->y;
				m_rooms[ t->session->roomIndex ]->userInfo[ t->index ].z = packet.z = t->z;
				m_rooms[ t->session->roomIndex ]->userInfo[ t->index ].angle = packet.angle = t->angle;

				// ������ �÷��̾ �����ϰ� ����
				BroadCastPacketExceptMe( m_rooms[ t->session->roomIndex ], &packet, t->index );

				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::ATTACK_PLAYER:
		{
			INGAME::AttackPlayerTask* t = reinterpret_cast<INGAME::AttackPlayerTask*>( task.second );
			if ( t != nullptr )
			{
				PACKET::SERVER_TO_CLIENT::AttackPlayerPacket packet;
				packet.index = t->index;

				// ������ �÷��̾ �����ϰ� ����
				BroadCastPacketExceptMe( m_rooms[ t->session->roomIndex ], &packet, t->index );

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

void GameManager::BroadCastPacketExceptMe( GameRoom* room, void* packet, int index )
{
	for ( auto& pl : room->userSessions )
	{
		if ( pl->key == index ) continue;
		MainServer::GetInstance().SendPacket( pl->socket, packet );
	}
}

int GameManager::PickSeeker( GameRoom* room )
{
	int temp = rand() % MAX_PLAYER_IN_ROOM;
	// ���� ����� ������ ���� �ʵ���
	while ( temp == room->currentSeeker )
	{
		temp = rand() % MAX_PLAYER_IN_ROOM;
	}

	return temp;
}
