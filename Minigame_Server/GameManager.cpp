

#include "AutoCall.hpp"
#include "GameManager.h"
#include "MainServer.h"
#include "LobbyManager.h"
#include "TimerManager.h"
#include "LogUtil.h"
#include <cmath>


GameManager::GameManager() {}

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
		case INGAME::ETaskType::CheckAlive:
		{
			INGAME::CheckAliveTask* t = reinterpret_cast<INGAME::CheckAliveTask*>( task.second );
			if ( t->m_session != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				Packet::ServerToClient::CheckAlivePacket packet;

				if ( MainServer::GetInstance().SendPacket( t->m_session, &packet ) != -1 )
				{
					TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + 10s, INGAME::ETaskType::CheckAlive,
						new INGAME::CheckAliveTask{ t->m_session } );
				}
			}
		}
		break;
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

				// �κ񿡼� �÷��̾� ����
				for ( auto& pl : t->m_room->m_userSessions )
				{
					LobbyManager::GetInstance().PushTask( Lobby::ETaskType::ExitLobby, new Lobby::ExitLobbyTask{ pl, roomNum } );
				}

				TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + WAIT_TIME, INGAME::ETaskType::RoundWait,
					new INGAME::RoundWaitTask{ t->m_room } );
				PRINT_LOG( "�� ���� ��û ����" );
			}
		}
		break;
		case INGAME::ETaskType::RoundWait:
		{
			INGAME::RoundWaitTask* t = reinterpret_cast< INGAME::RoundWaitTask* >( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				Packet::ServerToClient::RoundReadyPacket packet;
				int picked = _PickSeeker( t->m_room );

				if ( picked == -1 )
				{
					// ���� ���� ó�� �� �� ���� �½�ũ ���
					PushTask( INGAME::ETaskType::RoomRemove, new INGAME::RemoveRoomTask{ t->m_room } );
					break;
				}
				packet.m_seeker = t->m_room->m_currentSeeker;
				int count = 0;
				for ( auto& pl : t->m_room->m_userInfo )
				{
					if ( pl.first == packet.m_seeker ) continue;
					pl.second.m_object = rand() % NUM_OF_OBJECTS + 1;
					packet.m_hiderNum[ count ] = pl.first;
					packet.m_object[ count ] = pl.second.m_object;
					count++;
				}

				// �����鿡�� ���� �غ� �˸�
				_BroadCastPacket( t->m_room, &packet );
				TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + READY_TIME, INGAME::ETaskType::RoundReady,
					new INGAME::RoundReadyTask{ t->m_room, t->m_room->m_currentRound } );
				PRINT_LOG( "���� ���� - ���� �غ� ���·� ��ȯ" );
			}
		}
		break;
		case INGAME::ETaskType::RoundReady:
		{
			INGAME::RoundReadyTask* t = reinterpret_cast< INGAME::RoundReadyTask* >( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				// �ٸ� ������ ���尡 ������� �ʾҴٸ�
				if ( t->m_currentRound == t->m_room->m_currentRound )
				{
					if ( t->m_room->m_gameEnded ) break;
					t->m_room->m_roundStart = std::chrono::steady_clock::now();
					Packet::ServerToClient::RoundStartPacket packet;

					// �����鿡�� ���� ������ �˸�
					_BroadCastPacket( t->m_room, &packet );

					TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + EVENT_1_INTERVAL, INGAME::ETaskType::ProcessEvent,
						new INGAME::ProcessEventTask{ t->m_room, t->m_room->m_currentRound, 1 } );

					TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + GAME_TIME, INGAME::ETaskType::RoundResult,
						new INGAME::RoundResultTask{ t->m_room, t->m_room->m_currentRound, false } );
					PRINT_LOG( "���� ���� - ���� ���� ���·� ��ȯ" );
				}
			}
		}
		break;
		case INGAME::ETaskType::RoundEnd:
		{
			INGAME::RoundEndTask* t = reinterpret_cast< INGAME::RoundEndTask* >( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				// �ٸ� ������ ���尡 ������� �ʾҴٸ�
				if ( t->m_currentRound == t->m_room->m_currentRound )
				{
					// ������ ���尡 ������ �ʾҴٸ�
					if ( t->m_room->m_currentRound < MAX_ROUND )
					{
						t->m_room->m_currentRound++;

						Packet::ServerToClient::RoundReadyPacket packet;

						int picked = _PickSeeker( t->m_room );
						for ( auto& pl : t->m_room->m_userInfo )
						{
							// �÷��̾� ���� �ʱ�ȭ
							pl.second.m_isAlive = true;
						}
						t->m_room->m_gameEnded = false;

						if ( picked == -1 )
						{
							// ���� ���� ó�� �� �� ���� �½�ũ ���
							PushTask( INGAME::ETaskType::RoomRemove, new INGAME::RemoveRoomTask{ t->m_room } );
							break;
						}
						packet.m_seeker = t->m_room->m_currentSeeker;
						int count = 0;
						for ( auto& pl : t->m_room->m_userInfo )
						{
							if ( pl.first == packet.m_seeker ) continue;
							pl.second.m_object = rand() % NUM_OF_OBJECTS + 1;
							packet.m_hiderNum[ count ] = pl.first;
							packet.m_object[ count ] = pl.second.m_object;
							count++;
						}

						// �����鿡�� ���� �غ� �˸�
						_BroadCastPacket( t->m_room, &packet );

						TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + READY_TIME, INGAME::ETaskType::RoundReady,
							new INGAME::RoundReadyTask{ t->m_room, t->m_room->m_currentRound } );
						PRINT_LOG( "���� ���� - ���� ���� ���۵�" );
					}
					else
					{
						t->m_room->m_currentRound++;

						Packet::ServerToClient::GameEndPacket packet;

						// �κ� �÷��̾� ���
						for ( auto& pl : t->m_room->m_userSessions )
						{
							LobbyManager::GetInstance().PushTask( Lobby::ETaskType::EnterLobby,
								new Lobby::EnterLobbyTask{ pl, t->m_room->m_userInfo[ pl->m_key ].m_score } );
						}

						_BroadCastPacket( t->m_room, &packet );

						// ���� ���� ó�� �� �� ���� �½�ũ ���
						TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + GAME_TIME, INGAME::ETaskType::RoomRemove,
							new INGAME::RemoveRoomTask{ t->m_room } );
						PRINT_LOG( "���� ���� - ����" );
					}

				}
			}
		}
		break;
		case INGAME::ETaskType::RoomRemove:
		{
			INGAME::RemoveRoomTask* t = reinterpret_cast< INGAME::RemoveRoomTask* >( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				m_rooms.erase( t->m_room->m_roomNum );
				delete ( t->m_room );
				PRINT_LOG( "�� ���� ��û ����" );

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

						// ������ �÷��̾ �����ϰ� ����
						_BroadCastPacketExceptMe( m_rooms[ t->m_session->m_roomIndex ], &packet, t->m_index );
					}
				}

			}
		}
		break;
		case INGAME::ETaskType::AttackPlayer:
		{
			INGAME::AttackPlayerTask* t = reinterpret_cast< INGAME::AttackPlayerTask* >( task.second );
			if ( t->m_session != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				if ( t->m_session->m_roomIndex != -1 )
				{
					if ( m_rooms[ t->m_session->m_roomIndex ]->m_gameEnded ) break;
					auto& user = m_rooms[ t->m_session->m_roomIndex ]->m_userInfo[ t->m_index ];
					if ( _CheckPlayer( user ) )
					{
						Packet::ServerToClient::AttackPlayerPacket packet;
						packet.m_index = t->m_index;

						// ������ �÷��̾ �����ϰ� ����
						_BroadCastPacketExceptMe( m_rooms[ t->m_session->m_roomIndex ], &packet, t->m_index );

						for ( auto& pl : m_rooms[ t->m_session->m_roomIndex ]->m_userInfo )
						{
							if ( pl.first == t->m_index ) continue;
							if ( !pl.second.m_isAlive ) continue;
							// �Ÿ� �˻�
							double dx = pl.second.m_x - user.m_x;
							double dy = pl.second.m_y - user.m_y;
							double distance = sqrt( pow( dx, 2 ) + pow( dy, 2 ) );

							if ( distance < ATTACK_RANGE )
							{
								dx = dx / distance;
								dy = dy / distance;
								double x = cos( ( user.m_angle ) * 3.14 / 180 );
								double y = sin( ( user.m_angle ) * 3.14 / 180 );
								// �������� ���� ���Ϳ� �� ��ü������ ���⺤���� ���հ� ���
								double angle = atan2( x * dy - dx * y, dx * x + dy * y ) * 180 / 3.14;
								if ( abs( angle ) <= ATTACK_ANGLE )
								{
									Packet::ServerToClient::KillPlayerPacket p;
									p.m_killer = t->m_index;
									p.m_victim = pl.second.m_userNum;
									pl.second.m_isAlive = false;
									m_rooms[ t->m_session->m_roomIndex ]->m_aliveHider -= 1;
									auto time = duration_cast< seconds >
										( steady_clock::now() - m_rooms[ t->m_session->m_roomIndex ]->m_roundStart );
									pl.second.m_score += time.count() * SCORE_HIDERTIME;

									_BroadCastPacket( m_rooms[ t->m_session->m_roomIndex ], &p );
									PRINT_LOG( "���� ���� ��Ŷ ���۵�" );

									user.m_score += SCORE_SEEKERKILL;

									if ( m_rooms[ t->m_session->m_roomIndex ]->m_aliveHider == 0 )
									{
										// ���ο� ���� ����
										PushTask( INGAME::ETaskType::RoundResult, new INGAME::RoundResultTask{ m_rooms[ t->m_session->m_roomIndex ],
											m_rooms[ t->m_session->m_roomIndex ]->m_currentRound, true } );
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
			INGAME::RemovePlayerTask* t = reinterpret_cast< INGAME::RemovePlayerTask* >( task.second );
			if ( t->m_session != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				if ( t->m_session->m_roomIndex != -1 )
				{
					m_rooms[ t->m_roomindex ]->m_userInfo.erase( t->m_index );
					m_rooms[ t->m_roomindex ]->m_userSessions.erase( std::remove( m_rooms[ t->m_roomindex ]->m_userSessions.begin(),
						m_rooms[ t->m_roomindex ]->m_userSessions.end(), t->m_session ) );
					PRINT_LOG( "�÷��̾� ���� ��û ����" );
					Packet::ServerToClient::RemovePlayerIngamePacket packet;
					packet.m_index = t->m_index;

					_BroadCastPacket( m_rooms[ t->m_roomindex ], &packet );
				}
			}
		}
		break;
		case INGAME::ETaskType::RoundResult:
		{
			INGAME::RoundResultTask* t = reinterpret_cast< INGAME::RoundResultTask* >( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				// �ٸ� ������ ���尡 ������� �ʾҴٸ�
				if ( t->m_currentRound == t->m_room->m_currentRound )
				{
					t->m_room->m_gameEnded = true;
					Packet::ServerToClient::GameResultPacket packet;
					packet.m_isSeekerWin = t->m_isSeekerWin;

					if ( t->m_isSeekerWin )
					{
						// �������� �߰� ���� ����
						auto time = GAME_TIME - duration_cast< seconds >( steady_clock::now() - t->m_room->m_roundStart );
						t->m_room->m_userInfo[ t->m_room->m_currentSeeker ].m_score += SCORE_SEEKERWIN + time.count() * SCORE_SEEKERTIME;
						PRINT_LOG( "���� ��� - ���� �¸�" );
					}
					else
					{
						// ������ �繰���� �߰� ���� ����
						for ( auto& pl : t->m_room->m_userInfo )
						{
							if ( pl.first == t->m_room->m_currentSeeker )	continue;
							if ( !pl.second.m_isAlive )
							{
								pl.second.m_score += SCORE_HIDERWIN;
								continue;
							}
							auto time = duration_cast< seconds >( steady_clock::now() - t->m_room->m_roundStart );
							pl.second.m_score += SCORE_HIDERWIN + SCORE_HIDERSURVIVE + time.count() * SCORE_HIDERTIME;
							PRINT_LOG( "���� ��� - �繰 �¸�" );
						}
					}

					_BroadCastPacket( t->m_room, &packet );

					TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + INTERVAL_TIME, INGAME::ETaskType::RoundEnd,
						new INGAME::RoundEndTask{ t->m_room, t->m_room->m_currentRound } );
				}
			}
		}
		break;
		case INGAME::ETaskType::ProcessEvent:
		{
			INGAME::ProcessEventTask* t = reinterpret_cast< INGAME::ProcessEventTask* >( task.second );
			if ( t->m_room != nullptr )
			{
				Base::AutoCall defer( [&t]() { delete t; } );
				// �ٸ� ������ ���尡 ������� �ʾҴٸ�
				if ( t->m_currentRound == t->m_room->m_currentRound )
				{
					switch ( t->m_eventcount )
					{
					case 1:
					{
						int eventNum = rand() % EVENT_COUNT;
						Packet::ServerToClient::RandomEventPacket packet;
						packet.m_eventIndex = eventNum;

						_BroadCastPacket( t->m_room, &packet );

						TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + EVENT_2_INTERVAL, INGAME::ETaskType::ProcessEvent,
							new INGAME::ProcessEventTask{ t->m_room, t->m_room->m_currentRound, 2 } );
						PRINT_LOG( "�̺�Ʈ �߻� - 1��°" );
					}
						break;
					case 2:
					{
						Packet::ServerToClient::ChangeObjectPacket packet;
						int count = 0;

						for ( auto& pl : t->m_room->m_userInfo )
						{
							if ( pl.first == t->m_room->m_currentSeeker ) continue;
							pl.second.m_object = rand() % NUM_OF_OBJECTS + 1;
							packet.m_hiderNum[ count ] = pl.first;
							packet.m_object[ count ] = pl.second.m_object;
							count++;
						}

						_BroadCastPacket( t->m_room, &packet );

						TimerManager::GetInstance().PushTask( std::chrono::steady_clock::now() + EVENT_3_INTERVAL, INGAME::ETaskType::ProcessEvent,
							new INGAME::ProcessEventTask{ t->m_room, t->m_room->m_currentRound, 3 } );
						PRINT_LOG( "�̺�Ʈ �߻� - 2��°" );
					}
					break;
					case 3:
					{
						int eventNum = rand() % EVENT_COUNT;
						Packet::ServerToClient::RandomEventPacket packet;
						packet.m_eventIndex = eventNum;

						_BroadCastPacket( t->m_room, &packet );
						PRINT_LOG( "�̺�Ʈ �߻� - 3��°" );
					}
					break;
					}
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
		MainServer::GetInstance().SendPacket( pl, packet );
	}
}

void GameManager::_BroadCastPacketExceptMe( GameRoom* room, void* packet, int index )
{
	if ( room == nullptr ) return;
	for ( auto& pl : room->m_userSessions )
	{
		if ( pl->m_key == index ) continue;
		MainServer::GetInstance().SendPacket( pl, packet );
	}
}

int GameManager::_PickSeeker( GameRoom* room )
{
	if ( room == nullptr )					return -1;
	if ( room->m_userSessions.size() == 0 )	return -1;
	if ( room->m_userSessions.size() == 1 )	return 0;
	int temp = rand() % room->m_userSessions.size();
	// ���� ����� ������ ���� �ʵ���
	while ( room->m_userSessions[ temp ]->m_key == room->m_currentSeeker )
	{
		temp = rand() % room->m_userSessions.size();
	}
	room->m_currentSeeker = room->m_userSessions[ temp ]->m_key;
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
