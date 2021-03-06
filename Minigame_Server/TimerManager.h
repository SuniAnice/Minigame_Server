

#pragma once

#include "BaseManager.h"
#include <concurrent_priority_queue.h>


class TimerManager : public Base::TSingleton < TimerManager >,
	public BaseManager < INGAME::ETaskType >
{
public:
	TimerManager();
	virtual ~TimerManager();

	void PushTask( std::chrono::steady_clock::time_point time,
		INGAME::ETaskType type, void* task );

	virtual void ThreadFunc();
private:
	concurrency::concurrent_priority_queue < TimerEvent > m_timerQueue;
};

