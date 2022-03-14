

#pragma once

#include "Slngleton.h"
#include "global.h"
#include <thread>
#include <vector>


class MainServer : public Base::TSingleton < MainServer >
{
public:
	MainServer();
	~MainServer();

	void Run();

	void WorkerFunc();

	void SendPacket( SOCKET target, void* p );

	void DoRecv( Session* session );

private:
	void Init();
private:
	static constexpr int WORKER_THREADS = 4;

	SOCKET listenSocket;
	HANDLE hIOCP;

	std::vector< std::thread > workerThreads;
};
