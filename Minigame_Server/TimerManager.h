

#pragma once

#include "BaseManager.h"
#include <concurrent_priority_queue.h>


class TimerManager : public Base::TSingleton < TimerManager >,
	public BaseManager < INGAME::TASK_TYPE >
{
public:
	TimerManager();
	virtual ~TimerManager();

	void PushTask( std::chrono::system_clock::time_point time, 
		std::pair < INGAME::TASK_TYPE, void* > task );

	virtual void ThreadFunc();
private:
	concurrency::concurrent_priority_queue < TimerEvent > m_timerQueue;
};

