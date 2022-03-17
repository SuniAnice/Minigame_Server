

#include "LobbyManager.h"
#include "LogUtil.h"
#include "MainServer.h"
#include "MatchMaker.h"
#include "GameManager.h"


LobbyManager::LobbyManager() {}

LobbyManager::~LobbyManager() {}

void LobbyManager::ThreadFunc()
{
	std::pair <Lobby::ETaskType, void* > task;
	while ( true )
	{
		if ( !m_tasks.try_pop( task ) )
		{
			std::this_thread::sleep_for( 10ms );
			continue;
		}

		switch ( task.first )
		{
		case Lobby::ETaskType::UserAccept:
		{
			OverlappedExtended* over = reinterpret_cast<OverlappedExtended*>( task.second );
			if ( over != nullptr )
			{
				int id = _GetNewId( over->m_socket );
				CreateIoCompletionPort( reinterpret_cast<HANDLE>( over->m_socket ), m_handle, id, 0 );
				m_users[ id ]->m_overlapped = *over;
				m_users[ id ]->m_overlapped.m_opType = EOpType::Recv;
				m_users[ id ]->m_prevSize = 0;
				PRINT_LOG( "플레이어 Accept 성공" );
				MainServer::GetInstance().DoRecv( m_users[ id ] );
			}
		}
		break;
		case Lobby::ETaskType::UserLogin:
		{
			Lobby::LoginTask* t = reinterpret_cast< Lobby::LoginTask* >( task.second );
			if ( t != nullptr )
			{
				if ( !m_usernames.count( t->m_nickname ) )
				{
					m_usernames.insert( t->m_nickname );
					Packet::ServerToClient::LoginOkPacket okPacket;
					okPacket.m_index = t->m_id;
					MainServer::GetInstance().SendPacket( m_users[ t->m_id ]->m_socket, &okPacket );
					for ( auto& pl : m_users )
					{
						if ( pl.second == nullptr )
						{
							continue;
						}
						// 접속 중인 플레이어들의 정보 전송
						if ( pl.second->m_nickname.size() != 0 && pl.second->m_nickname != t->m_nickname && pl.second->m_roomIndex == -1 )
						{
							Packet::ServerToClient::AddPlayerPacket plpacket;
							wmemcpy( plpacket.m_nickname, m_users[ pl.first ]->m_nickname.c_str(), m_users[ pl.first ]->m_nickname.size() );
							MainServer::GetInstance().SendPacket( m_users[ t->m_id ]->m_socket, &plpacket );
						}
					}
					m_users[ t->m_id ]->m_nickname = t->m_nickname;
					Packet::ServerToClient::AddPlayerPacket packet;
					wmemcpy( packet.m_nickname, m_users[ t->m_id ]->m_nickname.c_str(), m_users[ t->m_id ]->m_nickname.size() );
					_BroadCastLobby( &packet );
					PRINT_LOG( "유저 로그인 성공" );
				}
				else
				{
					Packet::ServerToClient::LoginFailPacket packet;
					MainServer::GetInstance().SendPacket( m_users[ t->m_id ]->m_socket, &packet );
					PRINT_LOG( "유저 로그인 실패 - 아이디 중복" );
				}
				
				delete task.second;
			}
		}
			break;
		case Lobby::ETaskType::LobbyChat:
		{
			Lobby::ChatTask* t = reinterpret_cast<Lobby::ChatTask*>( task.second );
			if ( t != nullptr )
			{
				Packet::ServerToClient::LobbyChatPacket packet;
				wmemcpy( packet.m_nickname, m_users[ t->m_id ]->m_nickname.c_str(), m_users[ t->m_id ]->m_nickname.size() );
				wmemcpy( packet.m_message, t->m_message.c_str(), t->m_message.size() );
				_BroadCastLobby( &packet );
				delete task.second;
			}
		}
		break;
		case Lobby::ETaskType::UserLogout:
		{
			int* id = reinterpret_cast<int*>( task.second );
			if ( m_users.count( *id ) )
			{
				std::cout << *id << "번 플레이어 로그아웃" << std::endl;

				// 닉네임을 설정한 사람이면
				if ( m_users[ *id ]->m_nickname.size() )
				{
					m_usernames.erase( m_users[ *id ]->m_nickname );
					if ( m_users[ *id ]->m_roomIndex == -1 )
					{
						Packet::ServerToClient::RemovePlayerPacket packet;
						wmemcpy( packet.m_nickname, m_users[ *id ]->m_nickname.c_str(), m_users[ *id ]->m_nickname.size() );
						_BroadCastLobby( &packet );
						// 매칭을 돌리지 않았으면 바로 제거 가능
						if ( !m_users[ *id ]->m_isMatching )
						{
							auto t = m_users[ *id ];
							m_users.erase( *id );
							delete t;
							break;
						}
						else
						{
							// 매칭중이면 객체 삭제를 매치메이커에 위임
							MatchMaker::GetInstance().PushTask( Match::ETaskType::UserRemove, new Match::RemovePlayerTask{ m_users[ *id ] } );
						}
					}
					else
					{
						// 게임에 있다면 객체 삭제를 게임매니저에 위임
						GameManager::GetInstance().PushTask( INGAME::ETaskType::RemovePlayer,
							new INGAME::RemovePlayerTask{ m_users[ *id ]->m_roomIndex, *id , m_users[ *id ] } );
					}
				}
				m_users.erase( *id );
			}
		}
		break;
		case Lobby::ETaskType::EnterLobby:
		{
			Lobby::EnterLobbyTask* t = reinterpret_cast<Lobby::EnterLobbyTask*>( task.second );
			if ( t->m_session != nullptr )
			{
				t->m_session->m_roomIndex = -1;
				for ( auto& pl : m_users )
				{
					// 접속 중인 플레이어들의 정보 전송
					if ( pl.second->m_nickname.size() != 0 && pl.second->m_nickname != t->m_session->m_nickname )
					{
						Packet::ServerToClient::AddPlayerPacket plpacket;
						wmemcpy( plpacket.m_nickname, m_users[ pl.first ]->m_nickname.c_str(), m_users[ pl.first ]->m_nickname.size() );
						MainServer::GetInstance().SendPacket( t->m_session->m_socket, &plpacket );
					}
				}
				Packet::ServerToClient::AddPlayerPacket packet;
				wmemcpy( packet.m_nickname, t->m_session->m_nickname.c_str(), t->m_session->m_nickname.size() );
				_BroadCastLobby( &packet );
				delete task.second;
			}
		}
		break;
		case Lobby::ETaskType::ExitLobby:
		{
			Lobby::ExitLobbyTask* t = reinterpret_cast<Lobby::ExitLobbyTask*>( task.second );
			if ( t->m_session != nullptr )
			{
				t->m_session->m_roomIndex = t->m_roomNum;

				Packet::ServerToClient::RemovePlayerPacket packet;
				wmemcpy( packet.m_nickname, t->m_session->m_nickname.c_str(), t->m_session->m_nickname.size() );

				_BroadCastLobby( &packet );
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
	return m_users[ id ];
}

int LobbyManager::_GetNewId( const SOCKET& m_socket )
{
	for ( int i = 1; i < MAX_USER; ++i )
	{
		if ( !m_users.count( i ) )
		{
			m_users[ i ] = new Session();
			m_users[ i ]->m_key = i;
			m_users[ i ]->m_socket = m_socket;
			return i;
		}
	}
	return -1;
}

void LobbyManager::_BroadCastLobby( void* packet )
{
	for ( auto it = m_users.begin(); it != m_users.end(); it++ )
	{
		if ( it->second == nullptr )
		{
			it = m_users.erase( it );
			continue;
		}
		if ( it->second->m_nickname.size() && it->second->m_roomIndex == -1 )
		{
			MainServer::GetInstance().SendPacket( it->second->m_socket, packet );
		}
	}
}

bool LobbyManager::_FindUserName( const std::wstring& nickname )
{
	for ( auto& pl : m_users )
	{
		if ( pl.second->m_nickname == nickname )
			return true;
	}
	return false;
}
