

#include "LobbyManager.h"
#include "MainServer.h"


LobbyManager::LobbyManager()
{
	m_Thread = static_cast<std::thread> ( [&]() 
		{
			this->ThreadFunc();
		} );
}

LobbyManager::~LobbyManager()
{
	
}

void LobbyManager::PushTask( LOBBY::TASK_TYPE type, void* info )
{
	m_tasks.push( { type, info } );
}

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
			int id = GetNewId( over->socket );
			CreateIoCompletionPort( reinterpret_cast<HANDLE>( over->socket ), m_handle, id, 0 );
			m_users[ id ].overlapped = *over;
			std::cout << id <<"번 플레이어 Accept 성공" << std::endl;
			MainServer::GetInstance().DoRecv( &m_users[ id ] );
		}
		break;
		case LOBBY::TASK_TYPE::USER_LOGIN:
		{
			LOBBY::LoginTask* t = reinterpret_cast< LOBBY::LoginTask* >( task.second );
			m_users[ t->id ].nickname = t->nickname;
			PACKET::SERVER_TO_CLIENT::AddPlayerPacket packet;
			wmemcpy( packet.nickname, m_users[ t->id ].nickname.c_str(), m_users[ t->id ].nickname.size() );
			for ( auto& player : m_users )
			{
				MainServer::GetInstance().SendPacket( player.second.socket, &packet );
			}
			delete task.second;
		}
			break;
		case LOBBY::TASK_TYPE::LOBBY_CHAT:
		{
			LOBBY::ChatTask* t = reinterpret_cast<LOBBY::ChatTask*>( task.second );
			PACKET::SERVER_TO_CLIENT::LobbyChatPacket packet;
			wmemcpy( packet.nickname, m_users[ t->id ].nickname.c_str(), m_users[ t->id ].nickname.size() );
			wmemcpy( packet.message, t->message.c_str(), t->message.size() );
			for ( auto& player : m_users )
			{
				MainServer::GetInstance().SendPacket( player.second.socket, &packet );
			}
			delete task.second;
		}
		break;
		case LOBBY::TASK_TYPE::USER_LOGOUT:
		{
			int* id = reinterpret_cast<int*>( task.second );
			m_users.erase( *id );
			std::cout << *id << "번 플레이어 로그아웃" << std::endl;
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
	return &m_users[id];
}

int LobbyManager::GetNewId( SOCKET socket )
{
	for ( int i = 1; i < MAX_USER; ++i )
	{
		if ( !m_users.count( i ) )
		{
			m_users[ i ].state = USER_STATE::STATE_CONNECTED;
			m_users[ i ].key = i;
			m_users[ i ].socket = socket;
			return i;
		}
	}
	return -1;
}
