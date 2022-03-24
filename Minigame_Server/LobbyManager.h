

#pragma once


#include "BaseManager.h"
#include <unordered_set>


class LobbyManager : public Base::TSingleton < LobbyManager >,
	public BaseManager < Lobby::ETaskType >
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

	int m_newUserNum = 0;

	int _GetNewId(const SOCKET& m_socket);

	void _BroadCastLobby( void* packet );

	bool _FindUserName( const std::wstring& nickname );
};

