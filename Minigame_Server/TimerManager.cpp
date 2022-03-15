

#include "TimerManager.h"

TimerManager::TimerManager()
{
}

TimerManager::~TimerManager()
{
}

void TimerManager::PushTask( std::chrono::system_clock::time_point time,
	std::pair<INGAME::TASK_TYPE, void*> task )
{
	m_timerQueue.push( TimerEvent{ time, task } );
}

void TimerManager::ThreadFunc()
{
	TimerEvent ev;

	while ( true )
	{
		if ( !m_timerQueue.try_pop( ev ) )
		{
			std::this_thread::sleep_for(10ms);
			continue;
		}
		
		// 시간이 되지 않았으면 다시 넣음
		if ( ev.time > std::chrono::system_clock::now() )
		{
			m_timerQueue.push( ev );
			continue;
		}

	}
}
