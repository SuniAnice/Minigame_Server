

#pragma once


#include "BaseManager.h"


class GameManager : public Base::TSingleton < GameManager >,
	public BaseManager < INGAME::ETaskType >
{
public:
	GameManager();
	virtual ~GameManager();

	virtual void ThreadFunc();
private:
	std::unordered_map < unsigned int, GameRoom* > m_rooms;

	void _BroadCastPacket( GameRoom* room, void* packet );

	void _BroadCastPacketExceptMe( GameRoom* room, void* packet, int index );

	int _PickSeeker( GameRoom* room ); 

	bool _CheckPlayer( UserInfo& info );

	unsigned int _GetNewRoomNum();
};
