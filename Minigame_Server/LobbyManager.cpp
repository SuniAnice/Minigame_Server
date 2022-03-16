

#include "LobbyManager.h"
#include "LogUtil.h"
#include "MainServer.h"
#include "MatchMaker.h"
#include "GameManager.h"


LobbyManager::LobbyManager() {}

LobbyManager::~LobbyManager() {}

void LobbyManager::ThreadFunc()
{
	std::pair <LOBBY::TASK_TYPE, void* > task;
	while ( true )
	{
		if ( !m_tasks.try_pop( task ) )
		{
			std::this_thread::sleep_for( 10ms );
			continue;
		}

		switch ( task.first )
		{
		case LOBBY::TASK_TYPE::USER_ACCEPT:
		{
			OVERLAPPED_EXTENDED* over = reinterpret_cast<OVERLAPPED_EXTENDED*>( task.second );
			if ( over != nullptr )
			{
				int id = GetNewId( over->socket );
				CreateIoCompletionPort( reinterpret_cast<HANDLE>( over->socket ), m_handle, id, 0 );
				m_users[ id ]->overlapped = *over;
				PRINT_LOG( id + "번 플레이어 Accept 성공" );
				MainServer::GetInstance().DoRecv( m_users[ id ] );
			}
		}
		break;
		case LOBBY::TASK_TYPE::USER_LOGIN:
		{
			LOBBY::LoginTask* t = reinterpret_cast< LOBBY::LoginTask* >( task.second );
			if ( t != nullptr )
			{
				if ( !m_usernames.count( t->nickname ) )
				{
					m_usernames.insert( t->nickname );
					PACKET::SERVER_TO_CLIENT::LoginOkPacket okPacket;
					okPacket.index = t->id;
					MainServer::GetInstance().SendPacket( m_users[ t->id ]->socket, &okPacket );
					for ( auto& pl : m_users )
					{
						// 접속 중인 플레이어들의 정보 전송
						if ( pl.second->nickname.size() != 0 && pl.second->nickname != t->nickname && pl.second->roomIndex == -1 )
						{
							PACKET::SERVER_TO_CLIENT::AddPlayerPacket plpacket;
							wmemcpy( plpacket.nickname, m_users[ pl.first ]->nickname.c_str(), m_users[ pl.first ]->nickname.size() );
							MainServer::GetInstance().SendPacket( m_users[ t->id ]->socket, &plpacket );
						}
					}
					m_users[ t->id ]->nickname = t->nickname;
					PACKET::SERVER_TO_CLIENT::AddPlayerPacket packet;
					wmemcpy( packet.nickname, m_users[ t->id ]->nickname.c_str(), m_users[ t->id ]->nickname.size() );
					BroadCastLobby( &packet );
					PRINT_LOG( "유저 로그인 성공 : " + t->id );
				}
				else
				{
					PACKET::SERVER_TO_CLIENT::LoginFailPacket packet;
					MainServer::GetInstance().SendPacket( m_users[ t->id ]->socket, &packet );
					PRINT_LOG( "유저 로그인 실패 - 아이디 중복 : " + t->id );
				}
				
				delete task.second;
			}
		}
			break;
		case LOBBY::TASK_TYPE::LOBBY_CHAT:
		{
			LOBBY::ChatTask* t = reinterpret_cast<LOBBY::ChatTask*>( task.second );
			if ( t != nullptr )
			{
				PACKET::SERVER_TO_CLIENT::LobbyChatPacket packet;
				wmemcpy( packet.nickname, m_users[ t->id ]->nickname.c_str(), m_users[ t->id ]->nickname.size() );
				wmemcpy( packet.message, t->message.c_str(), t->message.size() );
				BroadCastLobby( &packet );
				delete task.second;
			}
		}
		break;
		case LOBBY::TASK_TYPE::USER_LOGOUT:
		{
			int* id = reinterpret_cast<int*>( task.second );
			if ( m_users.count( *id ) )
			{
				std::cout << *id << "번 플레이어 로그아웃" << std::endl;

				// 닉네임을 설정한 사람이면
				if ( m_users[ *id ]->nickname.size() )
				{
					m_usernames.erase( m_users[ *id ]->nickname );
					if ( m_users[ *id ]->roomIndex == -1 )
					{
						PACKET::SERVER_TO_CLIENT::RemovePlayerPacket packet;
						wmemcpy( packet.nickname, m_users[ *id ]->nickname.c_str(), m_users[ *id ]->nickname.size() );
						BroadCastLobby( &packet );
						// 매칭을 돌리지 않았으면 바로 제거 가능
						if ( !m_users[ *id ]->isMatching )
						{
							delete ( m_users[ *id ] );
						}
						else
						{
							// 매칭중이면 객체 삭제를 매치메이커에 위임
							MatchMaker::GetInstance().PushTask( MATCH::TASK_TYPE::USER_REMOVE, new MATCH::RemovePlayerTask{ m_users[ *id ] } );
						}
					}
					else
					{
						// 게임에 있다면 객체 삭제를 게임매니저에 위임
						GameManager::GetInstance().PushTask( INGAME::TASK_TYPE::REMOVE_PLAYER,
							new INGAME::RemovePlayerTask{ m_users[ *id ]->roomIndex, *id , m_users[ *id ] } );
					}
				}
				m_users.erase( *id );
			}
		}
		break;
		case LOBBY::TASK_TYPE::USER_ENTERLOBBY:
		{
			LOBBY::EnterLobbyTask* t = reinterpret_cast<LOBBY::EnterLobbyTask*>( task.second );
			if ( t != nullptr )
			{
				t->session->roomIndex = -1;
				for ( auto& pl : m_users )
				{
					// 접속 중인 플레이어들의 정보 전송
					if ( pl.second->nickname.size() != 0 && pl.second->nickname != t->session->nickname )
					{
						PACKET::SERVER_TO_CLIENT::AddPlayerPacket plpacket;
						wmemcpy( plpacket.nickname, m_users[ pl.first ]->nickname.c_str(), m_users[ pl.first ]->nickname.size() );
						MainServer::GetInstance().SendPacket( t->session->socket, &plpacket );
					}
				}
				PACKET::SERVER_TO_CLIENT::AddPlayerPacket packet;
				wmemcpy( packet.nickname, t->session->nickname.c_str(), t->session->nickname.size() );
				BroadCastLobby( &packet );
				delete task.second;
			}
		}
		break;
		case LOBBY::TASK_TYPE::USER_EXITLOBBY:
		{
			LOBBY::ExitLobbyTask* t = reinterpret_cast<LOBBY::ExitLobbyTask*>( task.second );
			if ( t != nullptr )
			{
				t->session->roomIndex = t->roomNum;

				PACKET::SERVER_TO_CLIENT::RemovePlayerPacket packet;
				wmemcpy( packet.nickname, t->session->nickname.c_str(), t->session->nickname.size() );

				BroadCastLobby( &packet );
				delete task.second;
			}
		}
		break;
		}
	}
}

void LobbyManager::SetHandle( const HANDLE& handle )
{
	m_handle = handle;
}

Session* LobbyManager::GetSession( const int id )
{
	return m_users[id];
}

int LobbyManager::GetNewId( const SOCKET& socket )
{
	for ( int i = 1; i < MAX_USER; ++i )
	{
		if ( !m_users.count( i ) )
		{
			m_users[ i ] = new Session();
			m_users[ i ]->key = i;
			m_users[ i ]->socket = socket;
			return i;
		}
	}
	return -1;
}

void LobbyManager::BroadCastLobby( void* packet )
{
	for ( auto& player : m_users )
	{
		if ( player.second->nickname.size() && player.second->roomIndex == -1 )
		{
			MainServer::GetInstance().SendPacket( player.second->socket, packet );
		}
	}
}

bool LobbyManager::FindUserName( const std::wstring& nickname )
{
	for ( auto& pl : m_users )
	{
		if ( pl.second->nickname == nickname )
			return true;
	}
	return false;
}
