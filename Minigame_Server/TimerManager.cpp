

#include "TimerManager.h"
#include "GameManager.h"

TimerManager::TimerManager()
{
}

TimerManager::~TimerManager()
{
}

void TimerManager::PushTask( std::chrono::steady_clock::time_point time,
	INGAME::ETaskType type, void* task )
{
	m_timerQueue.push( TimerEvent{ time, std::make_pair( type, task ) } );
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
		if ( ev.m_time > std::chrono::steady_clock::now() )
		{
			m_timerQueue.push( ev );
			std::this_thread::sleep_for( 10ms );
			continue;
		}

		// 시간이 되었다면 전달받은 이벤트를 되돌려줌
		GameManager::GetInstance().PushTask( ev.m_task.first, ev.m_task.second );

	}
}
