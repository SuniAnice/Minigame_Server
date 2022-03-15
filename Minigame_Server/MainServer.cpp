

#include "GameManager.h"
#include "MainServer.h"
#include "MatchMaker.h"
#include "LobbyManager.h"
#include "LogUtil.h"
#include <iostream>


MainServer::MainServer() : listenSocket(), hIOCP( nullptr )
{
	setlocale( LC_ALL, "korean" );
	std::wcout.imbue( std::locale( "korean" ) );
}

MainServer::~MainServer()
{
	closesocket( listenSocket );
	WSACleanup();
}

void MainServer::Init()
{
	WSADATA wsa;
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsa) != 0 )
	{
		PRINT_LOG( "WSAStartup" );
	}

	hIOCP = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if ( hIOCP == NULL )
	{
		PRINT_LOG( "CreateIoCompletionPort" );
	}

	listenSocket = WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED );
	if ( listenSocket == INVALID_SOCKET )
	{
		PRINT_LOG( "Socket" );
	}

	CreateIoCompletionPort( reinterpret_cast< HANDLE >( listenSocket ), hIOCP, 0, 0 );

	SOCKADDR_IN serverAddr;
	ZeroMemory( &serverAddr, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl( INADDR_ANY );
	serverAddr.sin_port = htons( PORT );
	if ( ::bind( listenSocket, (SOCKADDR*)&serverAddr, sizeof( serverAddr ) ) == SOCKET_ERROR )
	{
		PRINT_LOG( "Bind" );
	}

	if ( listen( listenSocket, SOMAXCONN ) == SOCKET_ERROR )
	{
		PRINT_LOG( "Listen" );
	}

	// 스레드 생성
	for ( int i = 0; i < WORKER_THREADS; i++ )
	{
		workerThreads.emplace_back( [&]() 
			{
				this->WorkerFunc();
			} );
	}

	std::cout << "MainServer : 스레드 생성 완료" << std::endl;

	LobbyManager::GetInstance();
	MatchMaker::GetInstance();
	GameManager::GetInstance();
}

void MainServer::Run()
{
	Init();

	OVERLAPPED_EXTENDED *over = new OVERLAPPED_EXTENDED;
	over->opType = OP_TYPE::OP_ACCEPT;
	memset( &over->overlapped, 0, sizeof( over->overlapped ) );
	SOCKET c_socket = WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED );
	over->socket = c_socket;
	BOOL ret = AcceptEx( listenSocket, c_socket, over->packetBuffer, 0, 32, 32, NULL, &over->overlapped );

	LobbyManager::GetInstance().SetHandle( hIOCP );

	for ( auto& th : workerThreads )
	{
		th.join();
	}
}

void MainServer::WorkerFunc()
{
	while ( true )
	{
		DWORD bytes_recved;
		ULONG_PTR ikey;
		WSAOVERLAPPED* over;

		BOOL ret = GetQueuedCompletionStatus( hIOCP, &bytes_recved, &ikey, &over, INFINITE );
		int key = static_cast<int>( ikey );

		OVERLAPPED_EXTENDED* overEx = reinterpret_cast<OVERLAPPED_EXTENDED*>( over );

		if ( FALSE == ret ) {
			if ( 0 == key ) {
				exit( -1 );
			}
			else
			{
				PRINT_LOG( "GQCS" );
				// 로그아웃 처리
				LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::USER_LOGOUT, &key );
				continue;
			}
		}
		if ( key != 0 && bytes_recved == 0 )
		{
			// 로그아웃 처리
			LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::USER_LOGOUT, &key );
			continue;
		}

		switch ( overEx->opType )
		{
		case OP_TYPE::OP_RECV:
		{
			Session* session = LobbyManager::GetInstance().GetSession( key );

			unsigned char* packet_ptr = overEx->packetBuffer;
			int data_bytes = bytes_recved + session->prevSize;
			int packet_size = packet_ptr[ 0 ];

			while ( data_bytes >= packet_size )
			{
				ProcessPacket( key, packet_ptr );
				data_bytes -= packet_size;
				packet_ptr += packet_size;
				if ( 0 >= data_bytes )	break;
				packet_size = packet_ptr[ 0 ];
			}

			session->prevSize = data_bytes;
			if ( data_bytes > 0 )
				memcpy( overEx->packetBuffer, packet_ptr, data_bytes );

			DoRecv( session );
		}
			break;
		case OP_TYPE::OP_SEND:
		{
			delete overEx;
		}
			break;
		case OP_TYPE::OP_ACCEPT:
		{
			LobbyManager::GetInstance().PushTask(LOBBY::TASK_TYPE::USER_ACCEPT, overEx );

			OVERLAPPED_EXTENDED* new_over = new OVERLAPPED_EXTENDED;
			SOCKET cSock = WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED );
			new_over->socket = cSock;
			new_over->opType = OP_TYPE::OP_ACCEPT;
			memset( &new_over->overlapped, 0, sizeof( new_over->overlapped ) );
			AcceptEx( listenSocket, cSock, new_over->packetBuffer, 0, 32, 32, NULL, &new_over->overlapped );
			
		}
			break;
		default:
		{
			PRINT_LOG( "잘못된 Operation Type입니다 : " + static_cast<int>( overEx->opType ));
		}
			break;
		}
	}
}

void MainServer::SendPacket( SOCKET& target, void* p )
{
	int p_size = reinterpret_cast<unsigned char*>( p )[ 0 ];
	OVERLAPPED_EXTENDED* overlapped = new OVERLAPPED_EXTENDED;
	overlapped->opType = OP_TYPE::OP_SEND;
	memset( &overlapped->overlapped, 0, sizeof( overlapped->overlapped ) );
	memcpy( &overlapped->packetBuffer, p, p_size );
	overlapped->wsaBuf.buf = reinterpret_cast<char*>( overlapped->packetBuffer );
	overlapped->wsaBuf.len = p_size;

	int ret = WSASend( target, &( overlapped->wsaBuf ), 1, NULL, 0, &overlapped->overlapped, NULL );
}

void MainServer::DoRecv( Session* session )
{
	memset( &session->overlapped, 0, sizeof( session->overlapped ) );
	session->overlapped.wsaBuf.buf = reinterpret_cast<char*>( session->overlapped.packetBuffer ) + session->prevSize;
	session->overlapped.wsaBuf.len = BUFFER_SIZE - session->prevSize;

	DWORD r_flag = 0;
	int ret = WSARecv( session->socket, &session->overlapped.wsaBuf, 1, NULL, &r_flag, &session->overlapped.overlapped, NULL );
}

void MainServer::ProcessPacket( int id, unsigned char* buffer )
{
	PACKETINFO::CLIENT_TO_SERVER type = static_cast< PACKETINFO::CLIENT_TO_SERVER >( buffer[ 1 ] );
	switch ( type )
	{
	case PACKETINFO::CLIENT_TO_SERVER::LOGIN:
	{
		PACKET::CLIENT_TO_SERVER::LoginPacket* p = reinterpret_cast<PACKET::CLIENT_TO_SERVER::LoginPacket*>( buffer );
		LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::USER_LOGIN, new LOBBY::LoginTask{ id, p->nickname } );
	}
	break;
	case PACKETINFO::CLIENT_TO_SERVER::LOBBYCHAT:
	{
		PACKET::CLIENT_TO_SERVER::LobbyChatPacket* p = reinterpret_cast<PACKET::CLIENT_TO_SERVER::LobbyChatPacket*>( buffer );
		LobbyManager::GetInstance().PushTask( LOBBY::TASK_TYPE::LOBBY_CHAT, new LOBBY::ChatTask{ id, p->message } );
	}
	break;
	case PACKETINFO::CLIENT_TO_SERVER::STARTMATCHING:
	{
		PACKET::CLIENT_TO_SERVER::StartMatchingPacket* p = reinterpret_cast<PACKET::CLIENT_TO_SERVER::StartMatchingPacket*>( buffer );
		MatchMaker::GetInstance().PushTask( MATCH::TASK_TYPE::USER_STARTMATCHING, new MATCH::StartMatchingTask{ LobbyManager::GetInstance().GetSession( id ) } );
	}
	break;
	case PACKETINFO::CLIENT_TO_SERVER::STOPMATCHING:
	{
		PACKET::CLIENT_TO_SERVER::StopMatchingPacket* p = reinterpret_cast<PACKET::CLIENT_TO_SERVER::StopMatchingPacket*>( buffer );
		MatchMaker::GetInstance().PushTask( MATCH::TASK_TYPE::USER_STOPMATCHING, new MATCH::StopMatchingTask{ LobbyManager::GetInstance().GetSession( id ) } );
	}
	break;
	case PACKETINFO::CLIENT_TO_SERVER::GAMEREADY:
	{
		PACKET::CLIENT_TO_SERVER::GameReadyPacket* p = reinterpret_cast< PACKET::CLIENT_TO_SERVER::GameReadyPacket* >( buffer );
		//GameManager::GetInstance().PushTask(INGAME::)
	}
	break;
	default:
	{
		PRINT_LOG( "알 수 없는 패킷 타입입니다 : " + buffer[ 1 ] );
	}
		break;
	}
}
