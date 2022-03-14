

#pragma once

#include "Slngleton.h"
#include "global.h"
#include <thread>
#include <concurrent_queue.h>


class MatchMaker : public Base::TSingleton < MatchMaker >
{
public:
	MatchMaker();
	virtual ~MatchMaker();

	void PushTask( MATCH::TASK_TYPE type, void* info );

	void ThreadFunc();
private:
	std::thread m_Thread;

	concurrency::concurrent_queue< std::pair < MATCH::TASK_TYPE, void* > > m_tasks;
};

