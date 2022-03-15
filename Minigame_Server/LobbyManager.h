

#pragma once


#include "BaseManager.h"
#include <unordered_map>


constexpr size_t MAX_USER = 100000;

class LobbyManager : public Base::TSingleton < LobbyManager >,
	public BaseManager < LOBBY::TASK_TYPE >
{
public:
	LobbyManager();
	virtual ~LobbyManager();

	void SetHandle( HANDLE handle );

	Session* GetSession(int id);
private:
	std::unordered_map < int, Session* > m_users;

	virtual void ThreadFunc();

	HANDLE m_handle;

	int GetNewId(SOCKET socket);

	void BroadCastLobby( void* packet );

	bool FindUserName( std::wstring nickname );
};

