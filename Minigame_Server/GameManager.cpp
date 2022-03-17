

#include "GameManager.h"
#include "MainServer.h"
#include "LobbyManager.h"
#include "TimerManager.h"
#include "LogUtil.h"
#include <cmath>


GameManager::GameManager()
{
}

GameManager::~GameManager()
{
	for ( auto& room : m_rooms )
	{
		delete room.second;
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
				unsigned int roomNum = GetNewRoomNum();
				m_rooms[ roomNum ] = ( t->room );
				m_rooms[ roomNum ]->roomNum = roomNum;
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
					LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::USER_EXITLOBBY, new LOBBY::ExitLobbyTask{ pl, roomNum } );
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
						pl.second.isFrozen = false;
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

						PACKET::SERVER_TO_CLIENT::GameEndPacket packet;

						BroadCastPacket( t->room, &packet );

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
				m_rooms.erase( t->room->roomNum );
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
				if ( t->session->roomIndex != -1 )
				{
					auto& user = m_rooms[ t->session->roomIndex ]->userInfo[ t->index ];
					if ( CheckPlayer( user ) )
					{
						PACKET::SERVER_TO_CLIENT::MovePlayerPacket packet;
						packet.index = t->index;
						user.x = packet.x = t->x;
						user.y = packet.y = t->y;
						user.z = packet.z = t->z;
						user.angle = packet.angle = t->angle;

						// 움직인 플레이어를 제외하고 전송
						BroadCastPacketExceptMe( m_rooms[ t->session->roomIndex ], &packet, t->index );
					}
				}

				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::ATTACK_PLAYER:
		{
			INGAME::AttackPlayerTask* t = reinterpret_cast<INGAME::AttackPlayerTask*>( task.second );
			if ( t != nullptr )
			{
				if ( t->session->roomIndex != -1 )
				{
					auto& user = m_rooms[ t->session->roomIndex ]->userInfo[ t->index ];
					if ( CheckPlayer( user ) )
					{
						PACKET::SERVER_TO_CLIENT::AttackPlayerPacket packet;
						packet.index = t->index;

						// 공격한 플레이어를 제외하고 전송
						BroadCastPacketExceptMe( m_rooms[ t->session->roomIndex ], &packet, t->index );

						for ( auto& pl : m_rooms[ t->session->roomIndex ]->userInfo )
						{
							if ( pl.first == t->index ) continue;
							if ( !pl.second.isAlive ) continue;
							// 거리 검사
							double dx = pl.second.x - user.x;
							double dy = pl.second.y - user.y;
							double distance = sqrt( pow( dx, 2 ) + pow( dy, 2 ) );

							if ( distance < ATTACK_RANGE )
							{
								dx = dx / distance;
								dy = dy / distance;
								double x = cos( ( user.angle ) * 3.14 / 180 );
								double y = sin( ( user.angle ) * 3.14 / 180 );
								// 공격자의 방향 벡터와 각 물체까지의 방향벡터의 사잇각 계산
								double angle = atan2( x * dy - dx * y, dx * x + dy * y ) * 180 / 3.14;
								if ( angle <= ATTACK_ANGLE && angle >= -ATTACK_ANGLE )
								{
									PACKET::SERVER_TO_CLIENT::KillPlayerPacket p;
									p.killer = t->index;
									p.victim = pl.second.userNum;
									pl.second.isAlive = false;
									m_rooms[ t->session->roomIndex ]->aliveHider -= 1;

									BroadCastPacket( m_rooms[ t->session->roomIndex ], &p );
									PRINT_LOG( "공격 성공 패킷 전송됨" );

									if ( m_rooms[ t->session->roomIndex ]->aliveHider == 0 )
									{
										// 새로운 라운드 시작
										TimerManager::GetInstance().PushTask( std::chrono::system_clock::now() + INTERVAL_TIME, INGAME::TASK_TYPE::ROUND_END,
											new INGAME::RoundEndTask{ m_rooms[ t->session->roomIndex ], m_rooms[ t->session->roomIndex ]->currentRound } );
									}
								}
							}
						}
					}
				}
				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::REMOVE_PLAYER:
		{
			INGAME::RemovePlayerTask* t = reinterpret_cast<INGAME::RemovePlayerTask*>( task.second );
			if ( t != nullptr )
			{
				if ( t->session->roomIndex != -1 )
				{
					m_rooms[ t->roomindex ]->userInfo.erase( t->index );
					m_rooms[ t->roomindex ]->userSessions.erase( std::remove( m_rooms[ t->roomindex ]->userSessions.begin(),
						m_rooms[ t->roomindex ]->userSessions.end(), t->session ) );
					PRINT_LOG( "플레이어 제거 요청 받음" );
					PACKET::SERVER_TO_CLIENT::RemovePlayerIngamePacket packet;
					packet.index = t->index;

					BroadCastPacket( m_rooms[ t->roomindex ], &packet );
				}
				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::FREEZE:
		{
			INGAME::FreezeTask* t = reinterpret_cast<INGAME::FreezeTask*>( task.second );
			if ( t != nullptr )
			{
				auto &user = m_rooms[ t->session->roomIndex ]->userInfo[ t->index ];
				if ( CheckPlayer( user ) )
				{
					user.isFrozen = true;

					PACKET::SERVER_TO_CLIENT::FreezePacket packet;
					packet.index = t->index;

					BroadCastPacket( m_rooms[ t->session->roomIndex ], &packet );
				}

				delete task.second;
			}
		}
		break;
		case INGAME::TASK_TYPE::UNFREEZE:
		{
			INGAME::UnfreezeTask* t = reinterpret_cast<INGAME::UnfreezeTask*>( task.second );
			if ( t != nullptr )
			{
				auto& user = m_rooms[ t->session->roomIndex ]->userInfo[ t->index ];
				if ( CheckPlayer( user ) )
				{
					m_rooms[ t->session->roomIndex ]->userInfo[ t->target ].isFrozen = false;

					PACKET::SERVER_TO_CLIENT::UnfreezePacket packet;
					packet.index = t->target;

					BroadCastPacket( m_rooms[ t->session->roomIndex ], &packet );
				}

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

bool GameManager::CheckPlayer( UserInfo& info )
{
	if ( info.isAlive && !info.isFrozen )		return true;
	else										return false;
}

unsigned int GameManager::GetNewRoomNum()
{
	for ( int i = 0; ; i++ )
	{
		if ( !m_rooms.count( i ) ) return i;
	}
}
