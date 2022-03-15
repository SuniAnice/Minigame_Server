

#pragma once

#include "BaseManager.h"
#include <list>


class MatchMaker : public Base::TSingleton < MatchMaker >,
	public BaseManager < MATCH::TASK_TYPE >
{
public:
	MatchMaker();
	virtual ~MatchMaker();

	virtual void ThreadFunc();
private:
	std::list < Session* > m_matchingUser;
};

