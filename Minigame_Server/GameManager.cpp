

#include "AutoCall.hpp"
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
	std::pair <INGAME::ETaskType, void* > task;
	while ( true )
	{
		if ( !m_tasks.try_pop( task ) )
		{
			std::this_thread::sleep_for( 10ms );
			continue;
		}
		switch ( task.first )
		{
		case INGAME::ETaskType::RoomCreate:
		{
			INGAME::CreateRoomTask* t = reinterpret_cast< INGAME::CreateRoomTask* >( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				unsigned int roomNum = _GetNewRoomNum();
				m_rooms[ roomNum ] = ( t->m_room );
				m_rooms[ roomNum ]->m_roomNum = roomNum;
				Packet::ServerToClient::GameMatchedPacket packet;
				for ( int i = 0; i < MAX_PLAYER_IN_ROOM; i++ )
				{
					packet.m_users[ i ].m_userNum = t->m_room->m_userSessions[ i ]->m_key;
					wmemcpy( packet.m_users[ i ].m_nickname, t->m_room->m_userSessions[ i ]->m_nickname.c_str(),
						t->m_room->m_userSessions[ i ]->m_nickname.size() );
				}
				_BroadCastPacket( t->m_room, &packet );

				// 로비에서 플레이어 제거
				for ( auto& pl : t->m_room->m_userSessions )
				{
					LobbyManager::GetInstance().PushTask( Lobby::ETaskType::ExitLobby, new Lobby::ExitLobbyTask{ pl, roomNum } );
				}

				TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + WAIT_TIME, INGAME::ETaskType::RoundWait,
					new INGAME::RoundWaitTask{ t->m_room } );
				PRINT_LOG("방 생성 요청 받음");
			}
		}
			break;
		case INGAME::ETaskType::RoundWait:
		{
			INGAME::RoundWaitTask* t = reinterpret_cast<INGAME::RoundWaitTask*>( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				Packet::ServerToClient::RoundReadyPacket packet;
				int picked = _PickSeeker( t->m_room );

				if ( picked == -1 )
				{
					// 게임 종료 처리 및 방 제거 태스크 등록
					PushTask( INGAME::ETaskType::RoomRemove, new INGAME::RemoveRoomTask{ t->m_room } );
					break;
				}
				packet.m_seeker = t->m_room->m_userSessions[ picked ]->m_key;

				// 유저들에게 라운드 준비를 알림
				_BroadCastPacket( t->m_room, &packet );
				TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + READY_TIME, INGAME::ETaskType::RoundReady,
					new INGAME::RoundReadyTask{ t->m_room, t->m_room->m_currentRound } );
				PRINT_LOG( "게임 상태 - 라운드 준비 상태로 전환" );
			}
		}
		break;
		case INGAME::ETaskType::RoundReady:
		{
			INGAME::RoundReadyTask* t = reinterpret_cast<INGAME::RoundReadyTask*>( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				// 다른 사유로 라운드가 종료되지 않았다면
				if ( t->m_currentRound == t->m_room->m_currentRound )
				{
					t->m_room->m_roundStert = std::chrono::steady_clock::now();
					Packet::ServerToClient::RoundStartPacket packet;

					// 유저들에게 라운드 시작을 알림
					_BroadCastPacket( t->m_room, &packet );

					TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + GAME_TIME, INGAME::ETaskType::RoundEnd,
						new INGAME::RoundEndTask{ t->m_room, t->m_room->m_currentRound } );
					PRINT_LOG( "게임 상태 - 라운드 진행 상태로 전환" );
				}
			}
		}
		break;
		case INGAME::ETaskType::RoundEnd:
		{
			INGAME::RoundEndTask* t = reinterpret_cast<INGAME::RoundEndTask*>( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				// 다른 사유로 라운드가 종료되지 않았다면
				if ( t->m_currentRound == t->m_room->m_currentRound )
				{
					// 설정된 라운드가 끝나지 않았다면
					if ( t->m_room->m_currentRound < MAX_ROUND )
					{
						t->m_room->m_currentRound++;

						Packet::ServerToClient::RoundReadyPacket packet;

						int picked = _PickSeeker( t->m_room );
						for ( auto& pl : t->m_room->m_userInfo )
						{
							// 플레이어 정보 초기화
							pl.second.m_isAlive = true;
						}

						if ( picked == -1 )
						{
							// 게임 종료 처리 및 방 제거 태스크 등록
							PushTask( INGAME::ETaskType::RoomRemove, new INGAME::RemoveRoomTask{ t->m_room } );
							break;
						}
						packet.m_seeker = t->m_room->m_userSessions[ picked ]->m_key;

						// 유저들에게 라운드 준비를 알림
						_BroadCastPacket( t->m_room, &packet );

						TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + READY_TIME, INGAME::ETaskType::RoundReady,
							new INGAME::RoundReadyTask{ t->m_room, t->m_room->m_currentRound } );
						PRINT_LOG( "게임 상태 - 다음 라운드 시작됨" );
					}
					else
					{
						t->m_room->m_currentRound++;

						Packet::ServerToClient::GameEndPacket packet;

						// 로비에 플레이어 등록
						for ( auto& pl : t->m_room->m_userSessions )
						{
							LobbyManager::GetInstance().PushTask( Lobby::ETaskType::EnterLobby, new Lobby::EnterLobbyTask{ pl } );
						}

						_BroadCastPacket( t->m_room, &packet );

						// 게임 종료 처리 및 방 제거 태스크 등록
						TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + GAME_TIME, INGAME::ETaskType::RoomRemove,
							new INGAME::RemoveRoomTask{ t->m_room } );
						PRINT_LOG( "게임 상태 - 종료" );
					}
					
				}
			}
		}
		break;
		case INGAME::ETaskType::RoomRemove:
		{
			INGAME::RemoveRoomTask* t = reinterpret_cast<INGAME::RemoveRoomTask*>( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				m_rooms.erase( t->m_room->m_roomNum );
				delete ( t->m_room );
				PRINT_LOG( "방 제거 요청 받음" );

			}
		}
		break;
		case INGAME::ETaskType::MovePlayer:
		{
			INGAME::MovePlayerTask* t = reinterpret_cast<INGAME::MovePlayerTask*>( task.second );
			if ( t->m_session != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				if ( t->m_session->m_roomIndex != -1 )
				{
					auto& user = m_rooms[ t->m_session->m_roomIndex ]->m_userInfo[ t->m_index ];
					if ( _CheckPlayer( user ) )
					{
						Packet::ServerToClient::MovePlayerPacket packet;
						packet.m_index = t->m_index;
						user.m_x = packet.m_x = t->m_x;
						user.m_y = packet.m_y = t->m_y;
						user.m_z = packet.m_z = t->m_z;
						user.m_angle = packet.m_angle = t->m_angle;

						// 움직인 플레이어를 제외하고 전송
						_BroadCastPacketExceptMe( m_rooms[ t->m_session->m_roomIndex ], &packet, t->m_index );
					}
				}

			}
		}
		break;
		case INGAME::ETaskType::AttackPlayer:
		{
			INGAME::AttackPlayerTask* t = reinterpret_cast<INGAME::AttackPlayerTask*>( task.second );
			if ( t->m_session != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				if ( t->m_session->m_roomIndex != -1 )
				{
					auto& user = m_rooms[ t->m_session->m_roomIndex ]->m_userInfo[ t->m_index ];
					if ( _CheckPlayer( user ) )
					{
						Packet::ServerToClient::AttackPlayerPacket packet;
						packet.m_index = t->m_index;

						// 공격한 플레이어를 제외하고 전송
						_BroadCastPacketExceptMe( m_rooms[ t->m_session->m_roomIndex ], &packet, t->m_index );

						for ( auto& pl : m_rooms[ t->m_session->m_roomIndex ]->m_userInfo )
						{
							if ( pl.first == t->m_index ) continue;
							if ( !pl.second.m_isAlive ) continue;
							// 거리 검사
							double dx = pl.second.m_x - user.m_x;
							double dy = pl.second.m_y - user.m_y;
							double distance = sqrt( pow( dx, 2 ) + pow( dy, 2 ) );

							if ( distance < ATTACK_RANGE )
							{
								dx = dx / distance;
								dy = dy / distance;
								double x = cos( ( user.m_angle ) * 3.14 / 180 );
								double y = sin( ( user.m_angle ) * 3.14 / 180 );
								// 공격자의 방향 벡터와 각 물체까지의 방향벡터의 사잇각 계산
								double angle = atan2( x * dy - dx * y, dx * x + dy * y ) * 180 / 3.14;
								if ( angle <= ATTACK_ANGLE && angle >= -ATTACK_ANGLE )
								{
									Packet::ServerToClient::KillPlayerPacket p;
									p.m_killer = t->m_index;
									p.m_victim = pl.second.m_userNum;
									pl.second.m_isAlive = false;
									m_rooms[ t->m_session->m_roomIndex ]->m_aliveHider -= 1;

									_BroadCastPacket( m_rooms[ t->m_session->m_roomIndex ], &p );
									PRINT_LOG( "공격 성공 패킷 전송됨" );

									if ( m_rooms[ t->m_session->m_roomIndex ]->m_aliveHider == 0 )
									{
										// 새로운 라운드 시작
										TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + INTERVAL_TIME, INGAME::ETaskType::RoundEnd,
											new INGAME::RoundEndTask{ m_rooms[ t->m_session->m_roomIndex ], m_rooms[ t->m_session->m_roomIndex ]->m_currentRound } );
									}
								}
							}
						}
					}
				}
			}
		}
		break;
		case INGAME::ETaskType::RemovePlayer:
		{
			INGAME::RemovePlayerTask* t = reinterpret_cast<INGAME::RemovePlayerTask*>( task.second );
			if ( t->m_session != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				if ( t->m_session->m_roomIndex != -1 )
				{
					m_rooms[ t->m_roomindex ]->m_userInfo.erase( t->m_index );
					m_rooms[ t->m_roomindex ]->m_userSessions.erase( std::remove( m_rooms[ t->m_roomindex ]->m_userSessions.begin(),
						m_rooms[ t->m_roomindex ]->m_userSessions.end(), t->m_session ) );
					PRINT_LOG( "플레이어 제거 요청 받음" );
					Packet::ServerToClient::RemovePlayerIngamePacket packet;
					packet.m_index = t->m_index;

					_BroadCastPacket( m_rooms[ t->m_roomindex ], &packet );
				}
			}
		}
		break;
		}
	}
}

void GameManager::_BroadCastPacket( GameRoom* room, void* packet )
{
	if ( room == nullptr ) return;
	for ( auto& pl : room->m_userSessions )
	{
		MainServer::GetInstance().SendPacket( pl->m_socket, packet );
	}
}

void GameManager::_BroadCastPacketExceptMe( GameRoom* room, void* packet, int index )
{
	if ( room == nullptr ) return;
	for ( auto& pl : room->m_userSessions )
	{
		if ( pl->m_key == index ) continue;
		MainServer::GetInstance().SendPacket( pl->m_socket, packet );
	}
}

int GameManager::_PickSeeker( GameRoom* room )
{
	if ( room == nullptr )					return -1;
	if ( room->m_userSessions.size() == 0 )	return -1;
	if ( room->m_userSessions.size() == 1 )	return 0;
	int temp = rand() % room->m_userSessions.size();
	// 같은 사람은 술래가 되지 않도록
	while ( temp == room->m_currentSeeker )
	{
		temp = rand() % room->m_userSessions.size();
	}
	room->m_currentSeeker = temp;
	room->m_aliveHider = room->m_userSessions.size() - SEEKER_COUNT;
	return temp;
}

bool GameManager::_CheckPlayer( UserInfo& info )
{
	if ( info.m_isAlive )						return true;
	else										return false;
}

unsigned int GameManager::_GetNewRoomNum()
{
	for ( int i = 0; ; i++ )
	{
		if ( !m_rooms.count( i ) ) return i;
	}
}
