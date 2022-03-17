

#pragma once


#include "Slngleton.h"
#include "global.h"
#include <thread>


class MainServer : public Base::TSingleton < MainServer >
{
public:
	MainServer();
	~MainServer();

	void Run();

	void DoRecv( Session* session );

	void SendPacket( SOCKET& target, void* p );
private:
	void _Init();
private:
	static constexpr int WORKER_THREADS = 4;

	SOCKET m_listenSocket;

	HANDLE m_hIOCP;

	std::vector< std::thread > m_workerThreads;

	void _ProcessPacket( int id, unsigned char* over );

	void _WorkerFunc();
};
