

#include "AutoCall.hpp"
#include "DBManager.h"
#include "GameManager.h"
#include "MainServer.h"
#include "MatchMaker.h"
#include "TimerManager.h"
#include "LobbyManager.h"
#include "LogUtil.h"
#include <iostream>


MainServer::MainServer() : m_listenSocket(), m_hIOCP( nullptr )
{
	setlocale( LC_ALL, "korean" );
	std::wcout.imbue( std::locale( "korean" ) );
}

MainServer::~MainServer()
{
	closesocket( m_listenSocket );
	WSACleanup();
}

void MainServer::_Init()
{
	WSADATA wsa;
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsa) != 0 )
	{
		PRINT_LOG( "WSAStartup" );
	}

	m_hIOCP = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if ( m_hIOCP == NULL )
	{
		PRINT_LOG( "CreateIoCompletionPort" );
	}

	m_listenSocket = WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED );
	if ( m_listenSocket == INVALID_SOCKET )
	{
		PRINT_LOG( "Socket" );
	}

	CreateIoCompletionPort( reinterpret_cast< HANDLE >( m_listenSocket ), m_hIOCP, 0, 0 );

	SOCKADDR_IN serverAddr;
	ZeroMemory( &serverAddr, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl( INADDR_ANY );
	serverAddr.sin_port = htons( PORT );


	if ( ::bind( m_listenSocket, (SOCKADDR*)&serverAddr, sizeof( serverAddr ) ) == SOCKET_ERROR )
	{
		PRINT_LOG( "Bind" );
	}



	if ( listen( m_listenSocket, SOMAXCONN ) == SOCKET_ERROR )
	{
		PRINT_LOG( "Listen" );
	}

	// 스레드 생성
	for ( int i = 0; i < WORKER_THREADS; i++ )
	{
		m_workerThreads.emplace_back( [&]() 
			{
				this->_WorkerFunc();
			} );
	}

	std::cout << "MainServer : 스레드 생성 완료" << std::endl;

	LobbyManager::GetInstance();
	MatchMaker::GetInstance();
	GameManager::GetInstance();
	TimerManager::GetInstance();
	DBManager::GetInstance();

	srand( time( NULL ) );
}

void MainServer::Run()
{
	_Init();

	OverlappedExtended *over = new OverlappedExtended;
	over->m_opType = EOpType::Accept;
	memset( &over->m_overlapped, 0, sizeof( over->m_overlapped ) );
	SOCKET c_socket = WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED );
	over->m_socket = c_socket;
	BOOL ret = AcceptEx( m_listenSocket, c_socket, over->m_packetBuffer, 0, 32, 32, NULL, &over->m_overlapped );

	LobbyManager::GetInstance().SetHandle( m_hIOCP );

	for ( auto& th : m_workerThreads )
	{
		th.join();
	}
}

void MainServer::_WorkerFunc()
{
	while ( true )
	{
		DWORD bytesReceived;
		ULONG_PTR iKey;
		WSAOVERLAPPED* over;

		BOOL ret = GetQueuedCompletionStatus( m_hIOCP, &bytesReceived, &iKey, &over, INFINITE );
		int key = static_cast<int>( iKey );

		OverlappedExtended* overEx = reinterpret_cast< OverlappedExtended* >( over );

		if ( FALSE == ret ) {
			if ( 0 == key ) {
				exit( -1 );
			}
			else
			{
				PRINT_LOG( "GQCS" );
				// 로그아웃 처리
				LobbyManager::GetInstance().PushTask( Lobby::ETaskType::UserLogout, &key );
				continue;
			}
		}
		if ( key != 0 && bytesReceived <= 0 )
		{
			// 로그아웃 처리
			LobbyManager::GetInstance().PushTask( Lobby::ETaskType::UserLogout, &key );
			continue;
		}

		switch ( overEx->m_opType )
		{
		case EOpType::Recv:
		{
			Session* session = LobbyManager::GetInstance().GetSession( key );
			if ( session == nullptr ) continue;
			unsigned char* packetPtr = overEx->m_packetBuffer;
			int dataBytes = bytesReceived + session->m_prevSize;
			int packetSize = packetPtr[ 0 ];

			while ( dataBytes >= packetSize )
			{
				_ProcessPacket( key, packetPtr );
				dataBytes -= packetSize;
				packetPtr += packetSize;
				if ( 0 >= dataBytes )	break;
				packetSize = packetPtr[ 0 ];
				if ( packetSize <= 0 || packetSize >= BUFFER_SIZE )
				{
					// 비정상 패킷 수신
					ZeroMemory( overEx->m_packetBuffer, BUFFER_SIZE );
				}
			}

			session->m_prevSize = dataBytes;
			if ( dataBytes > 0 )
			{
				memcpy( overEx->m_packetBuffer, packetPtr, dataBytes );
			}

			DoRecv( session );
		}
			break;
		case EOpType::Send:
		{
			delete overEx;
		}
			break;
		case EOpType::Accept:
		{
			LobbyManager::GetInstance().PushTask( Lobby::ETaskType::UserAccept, overEx );

			OverlappedExtended* newOver = new OverlappedExtended;
			SOCKET cSock = WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED );
			newOver->m_socket = cSock;
			newOver->m_opType = EOpType::Accept;
			memset( &newOver->m_overlapped, 0, sizeof( newOver->m_overlapped ) );
			AcceptEx( m_listenSocket, cSock, newOver->m_packetBuffer, 0, 32, 32, NULL, &newOver->m_overlapped );
		}
			break;
		default:
		{
			PRINT_LOG( "잘못된 Operation Type입니다");
			ZeroMemory( overEx->m_packetBuffer, BUFFER_SIZE );
			continue;
		}
			break;
		}
	}
}

int MainServer::SendPacket( Session* target, void* p )
{
	if ( target == nullptr ) return -1;
	int p_size = reinterpret_cast< unsigned char* >( p )[ 0 ];
	OverlappedExtended* m_overlapped = new OverlappedExtended;
	m_overlapped->m_opType = EOpType::Send;
	memset( &m_overlapped->m_overlapped, 0, sizeof( m_overlapped->m_overlapped ) );
	memcpy( &m_overlapped->m_packetBuffer, p, p_size );
	m_overlapped->m_wsaBuf.buf = reinterpret_cast< char* >( m_overlapped->m_packetBuffer );
	m_overlapped->m_wsaBuf.len = p_size;

	int ret = WSASend( target->m_socket, &( m_overlapped->m_wsaBuf ),
		1, NULL, 0, &m_overlapped->m_overlapped, NULL );
	if ( ret == SOCKET_ERROR )
	{
		LobbyManager::GetInstance().PushTask( Lobby::ETaskType::UserLogout, &target->m_key );
		PRINT_LOG( "send failed : invalid player" );
	}
	return ret;
}

void MainServer::DoRecv( Session* session )
{
	session->m_overlapped.m_wsaBuf.buf = 
		reinterpret_cast< char* >( session->m_overlapped.m_packetBuffer ) + session->m_prevSize;
	session->m_overlapped.m_wsaBuf.len = BUFFER_SIZE - session->m_prevSize;
	memset( &session->m_overlapped.m_overlapped, 0,
		sizeof( session->m_overlapped.m_overlapped ) );

	DWORD rFlag = 0;
	int ret = WSARecv( session->m_socket, &session->m_overlapped.m_wsaBuf,
		1, NULL, &rFlag, &session->m_overlapped.m_overlapped, NULL );
}

void MainServer::_ProcessPacket( int id, unsigned char* buffer )
{
	PacketInfo::EClientToServer type = static_cast< PacketInfo::EClientToServer >( buffer[ 1 ] );
	switch ( type )
	{
	case PacketInfo::EClientToServer::Login:
	{
		Packet::ClientToServer::LoginPacket* p = 
			reinterpret_cast< Packet::ClientToServer::LoginPacket* >( buffer );
		LobbyManager::GetInstance().PushTask( Lobby::ETaskType::UserLogin,
			new Lobby::LoginTask{ id, p->m_nickname } );
	}
	break;
	case PacketInfo::EClientToServer::LobbyChat:
	{
		Packet::ClientToServer::LobbyChatPacket* p = 
			reinterpret_cast< Packet::ClientToServer::LobbyChatPacket* >( buffer );
		LobbyManager::GetInstance().PushTask( Lobby::ETaskType::LobbyChat,
			new Lobby::ChatTask{ id, p->m_message } );
	}
	break;
	case PacketInfo::EClientToServer::StartMatching:
	{
		Packet::ClientToServer::StartMatchingPacket* p = 
			reinterpret_cast< Packet::ClientToServer::StartMatchingPacket* >( buffer );
		MatchMaker::GetInstance().PushTask( Match::ETaskType::StartMatching,
			new Match::StartMatchingTask{ LobbyManager::GetInstance().GetSession( id ) } );
	}
	break;
	case PacketInfo::EClientToServer::StopMatching:
	{
		Packet::ClientToServer::StopMatchingPacket* p = 
			reinterpret_cast< Packet::ClientToServer::StopMatchingPacket* >( buffer );
		MatchMaker::GetInstance().PushTask( Match::ETaskType::StopMatching,
			new Match::StopMatchingTask{ LobbyManager::GetInstance().GetSession( id ) } );
	}
	break;
	case PacketInfo::EClientToServer::MovePlayer:
	{
		Packet::ClientToServer::PlayerMovePacket* p = 
			reinterpret_cast< Packet::ClientToServer::PlayerMovePacket* >( buffer );
		GameManager::GetInstance().PushTask( INGAME::ETaskType::MovePlayer,
			new INGAME::MovePlayerTask{ LobbyManager::GetInstance().GetSession( id ), id, p->m_x, p->m_y, p->m_z, p->m_angle } );
	}
	break;
	case PacketInfo::EClientToServer::Attack:
	{
		Packet::ClientToServer::PlayerAttackPacket* p = 
			reinterpret_cast< Packet::ClientToServer::PlayerAttackPacket* >( buffer );
		GameManager::GetInstance().PushTask( INGAME::ETaskType::AttackPlayer,
			new INGAME::AttackPlayerTask{ LobbyManager::GetInstance().GetSession( id ), id } );
	}
	break;
	case PacketInfo::EClientToServer::MoveToLobby:
	{
		Packet::ClientToServer::MoveToLobbyPacket* p =
			reinterpret_cast<Packet::ClientToServer::MoveToLobbyPacket*>( buffer );
		LobbyManager::GetInstance().PushTask( Lobby::ETaskType::OfferLobbyInfo,
			new Lobby::OfferLobbyInfoTask{ LobbyManager::GetInstance().GetSession( id ) } );
	}
	break;
	default:
	{
		PRINT_LOG( "알 수 없는 패킷 타입입니다" );
	}
	break;
	}
}
