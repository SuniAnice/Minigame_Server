

#pragma once


#include "BaseManager.h"


class GameManager : public Base::TSingleton < GameManager >,
	public BaseManager < INGAME::TASK_TYPE >
{
public:
	GameManager();
	virtual ~GameManager();

	virtual void ThreadFunc();
private:
	std::vector < GameRoom* > m_rooms;
};
