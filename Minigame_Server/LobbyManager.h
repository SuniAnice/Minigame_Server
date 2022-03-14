

#pragma once

#include "Slngleton.h"
#include "global.h"
#include <array>
#include <unordered_map>
#include <thread>
#include <concurrent_queue.h>


constexpr size_t MAX_USER = 100000;

class LobbyManager : public Base::TSingleton < LobbyManager >
{
public:
	LobbyManager();
	virtual ~LobbyManager();

	void PushTask( LOBBY::TASK_TYPE type, void* info );

	void SetHandle( HANDLE handle );

	Session* GetSession(int id);
private:
	std::unordered_map < int, Session > m_users;

	void ThreadFunc();

	HANDLE m_handle;

	std::thread m_Thread;

	int GetNewId(SOCKET socket);

	concurrency::concurrent_queue< std::pair < LOBBY::TASK_TYPE, void* > > m_tasks;

	void BroadCastLobby( void* packet );
};

