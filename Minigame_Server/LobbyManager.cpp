

#include "LobbyManager.h"
#include "LogUtil.h"
#include "MainServer.h"
#include "MatchMaker.h"


LobbyManager::LobbyManager() {}

LobbyManager::~LobbyManager() {}

void LobbyManager::ThreadFunc()
{
	std::pair <LOBBY::TASK_TYPE, void* > task;
	while ( true )
	{
		if ( !m_tasks.try_pop( task ) )
		{
			std::this_thread::yield();
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
				if ( !FindUserName( t->nickname ) )
				{
					PACKET::SERVER_TO_CLIENT::LoginOkPacket okPacket;
					okPacket.index = t->id;
					MainServer::GetInstance().SendPacket( m_users[ t->id ]->socket, &okPacket );
					for ( auto& pl : m_users )
					{
						// 접속 중인 플레이어들의 정보 전송
						if ( pl.second->nickname.size() != 0 && pl.second->nickname != t->nickname )
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
			if ( id != nullptr )
			{
				std::cout << *id << "번 플레이어 로그아웃" << std::endl;

				// 닉네임을 설정한 사람이면
				if ( m_users[ *id ]->nickname.size() )
				{
					PACKET::SERVER_TO_CLIENT::RemovePlayerPacket packet;
					wmemcpy( packet.nickname, m_users[ *id ]->nickname.c_str(), m_users[ *id ]->nickname.size() );
					BroadCastLobby( &packet );
				}
				delete ( m_users[ *id ] );
				m_users.erase( *id );
			}
		}
		break;
		}
	}
}

void LobbyManager::SetHandle( HANDLE handle )
{
	m_handle = handle;
}

Session* LobbyManager::GetSession( int id )
{
	return m_users[id];
}

int LobbyManager::GetNewId( SOCKET socket )
{
	for ( int i = 1; i < MAX_USER; ++i )
	{
		if ( !m_users.count( i ) )
		{
			m_users[ i ] = new Session();
			m_users[ i ]->state = USER_STATE::STATE_CONNECTED;
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
		if ( player.second->nickname.size() )
		{
			MainServer::GetInstance().SendPacket( player.second->socket, packet );
		}
	}
}

bool LobbyManager::FindUserName( std::wstring nickname )
{
	for ( auto& pl : m_users )
	{
		if ( pl.second->nickname == nickname )
			return true;
	}
	return false;
}
