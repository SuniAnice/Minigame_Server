

#pragma once


#include "BaseManager.h"
#include <unordered_map>
#include <unordered_set>


class LobbyManager : public Base::TSingleton < LobbyManager >,
	public BaseManager < LOBBY::TASK_TYPE >
{
public:
	LobbyManager();
	virtual ~LobbyManager();

	void SetHandle( const HANDLE& handle );

	Session* GetSession( const int id );
private:
	std::unordered_map < int, Session* > m_users;

	std::unordered_set < std::wstring > m_usernames;

	virtual void ThreadFunc();

	HANDLE m_handle;

	int GetNewId(const SOCKET& socket);

	void BroadCastLobby( void* packet );

	bool FindUserName( const std::wstring& nickname );
};

