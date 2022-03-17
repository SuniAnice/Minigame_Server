

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

				// 로비에서 플레이어 제거
				for ( auto& pl : t->room->userSessions )
				{
					LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::USER_EXITLOBBY, new LOBBY::ExitLobbyTask{ pl, m_rooms.size() - 1 } );
				}

				TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + WAIT_TIME, INGAME::TASK_TYPE::ROUND_WAIT, new INGAME::RoundWaitTask{ t->room } );
				PRINT_LOG("방 생성 요청 받음");
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
				int picked = PickSeeker( t->room );

				if ( picked == -1 )
				{
					// 게임 종료 처리 및 방 제거 태스크 등록
					PushTask( INGAME::TASK_TYPE::ROOM_REMOVE, new INGAME::RemoveRoomTask{ t->room } );
					break;
				}
				packet.seeker = t->room->userSessions[ picked ]->key;

				// 유저들에게 라운드 준비를 알림
				BroadCastPacket( t->room, &packet );
				TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + READY_TIME, INGAME::TASK_TYPE::ROUND_READY, new INGAME::RoundReadyTask{ t->room, t->room->currentRound } );
				PRINT_LOG( "게임 상태 - 라운드 준비 상태로 전환" );
				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::ROUND_READY:
		{
			INGAME::RoundReadyTask* t = reinterpret_cast<INGAME::RoundReadyTask*>( task.second );
			if ( t != nullptr )
			{
				// 다른 사유로 라운드가 종료되지 않았다면
				if ( t->currentRound == t->room->currentRound )
				{
					for ( auto& pl : t->room->userInfo )
					{
						// 플레이어 정보 초기화
						pl.second.isAlive = true;
					}
					PACKET::SERVER_TO_CLIENT::RoundStartPacket packet;

					// 유저들에게 라운드 시작을 알림
					BroadCastPacket( t->room, &packet );

					TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + GAME_TIME, INGAME::TASK_TYPE::ROUND_END, new INGAME::RoundEndTask{ t->room, t->room->currentRound } );
					PRINT_LOG( "게임 상태 - 라운드 진행 상태로 전환" );
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
				// 다른 사유로 라운드가 종료되지 않았다면
				if ( t->currentRound == t->room->currentRound )
				{
					// 설정된 라운드가 끝나지 않았다면
					if ( t->room->currentRound < MAX_ROUND )
					{
						t->room->currentRound++;

						PACKET::SERVER_TO_CLIENT::RoundReadyPacket packet;

						int picked = PickSeeker( t->room );

						if ( picked == -1 )
						{
							// 게임 종료 처리 및 방 제거 태스크 등록
							PushTask( INGAME::TASK_TYPE::ROOM_REMOVE, new INGAME::RemoveRoomTask{ t->room } );
							break;
						}
						packet.seeker = t->room->userSessions[ picked ]->key;

						// 유저들에게 라운드 준비를 알림
						BroadCastPacket( t->room, &packet );

						TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + READY_TIME, INGAME::TASK_TYPE::ROUND_READY, new INGAME::RoundReadyTask{ t->room, t->room->currentRound } );
						PRINT_LOG( "게임 상태 - 다음 라운드 시작됨" );
					}
					else
					{
						t->room->currentRound++;

						// 로비에 플레이어 등록
						for ( auto& pl : t->room->userSessions )
						{
							LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::USER_ENTERLOBBY, new LOBBY::EnterLobbyTask{ pl } );
						}

						// 게임 종료 처리 및 방 제거 태스크 등록
						TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + GAME_TIME, INGAME::TASK_TYPE::ROOM_REMOVE, new INGAME::RemoveRoomTask{ t->room } );
						PRINT_LOG( "게임 상태 - 종료" );
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
				delete ( t->room );
				PRINT_LOG( "방 제거 요청 받음" );

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

				// 움직인 플레이어를 제외하고 전송
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

				// 공격한 플레이어를 제외하고 전송
				BroadCastPacketExceptMe( m_rooms[ t->session->roomIndex ], &packet, t->index );

				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::REMOVE_PLAYER:
		{
			INGAME::RemovePlayerTask* t = reinterpret_cast<INGAME::RemovePlayerTask*>( task.second );
			if ( t != nullptr )
			{
				m_rooms[ t->roomindex ]->userInfo.erase( t->index );
				m_rooms[ t->roomindex ]->userSessions.erase( std::remove( m_rooms[ t->roomindex ]->userSessions.begin(),
					m_rooms[ t->roomindex ]->userSessions.end(), t->session ) );
				PRINT_LOG( "플레이어 제거 요청 받음" );
				PACKET::SERVER_TO_CLIENT::RemovePlayerIngamePacket packet;
				packet.index = t->index;

				BroadCastPacket( m_rooms[ t->roomindex ], &packet );

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
	if ( room->userSessions.size() == 0 ) return -1;
	if ( room->userSessions.size() == 1 ) return 0;
	int temp = rand() % room->userSessions.size();
	// 같은 사람은 술래가 되지 않도록
	while ( temp == room->currentSeeker )
	{
		temp = rand() % room->userSessions.size();
	}
	room->currentSeeker = temp;
	room->aliveHider = room->userSessions.size() - SEEKER_COUNT;
	return temp;
}
