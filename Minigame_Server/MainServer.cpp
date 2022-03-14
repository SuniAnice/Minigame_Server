

#include "MainServer.h"
#include "LobbyManager.h"
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
		std::cout << "WSAStartup" << std::endl;
	}

	hIOCP = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if ( hIOCP == NULL )
	{
		std::cout << "CreateIoCompletionPort" << std::endl;
	}

	listenSocket = WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED );
	if ( listenSocket == INVALID_SOCKET )
	{
		std::cout << "socket" << std::endl;
	}

	CreateIoCompletionPort( reinterpret_cast< HANDLE >( listenSocket ), hIOCP, 0, 0 );

	SOCKADDR_IN serverAddr;
	ZeroMemory( &serverAddr, sizeof( serverAddr ) );
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl( INADDR_ANY );
	serverAddr.sin_port = htons( PORT );
	if ( ::bind( listenSocket, (SOCKADDR*)&serverAddr, sizeof( serverAddr ) ) == SOCKET_ERROR )
	{
		std::cout << "bind" << std::endl;
	}

	if ( listen( listenSocket, SOMAXCONN ) == SOCKET_ERROR )
	{
		std::cout << "listen" << std::endl;
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
				std::cout << "GQCS" << std::endl;
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
			std::cout << "잘못된 Operation Type입니다 : " << static_cast< int >( overEx->opType ) << std::endl;
		}
			break;
		}
	}
}

void MainServer::SendPacket( SOCKET target, void* p )
{
	//int p_size = reinterpret_cast<unsigned char*>( p )[ 0 ];
	OVERLAPPED_EXTENDED* overlapped = new OVERLAPPED_EXTENDED;
	overlapped->opType = OP_TYPE::OP_SEND;
	memset( &overlapped->overlapped, 0, sizeof( overlapped->overlapped ) );
	memcpy( &overlapped->packetBuffer, p, BUFFER_SIZE );
	overlapped->wsaBuf.buf = reinterpret_cast<char*>( overlapped->packetBuffer );
	overlapped->wsaBuf.len = BUFFER_SIZE;

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

void MainServer::ProcessPacket( int id, unsigned char* packet )
{
	switch ( packet[ 1 ] )
	{

	}
}
