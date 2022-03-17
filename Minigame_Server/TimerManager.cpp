

#include "TimerManager.h"
#include "GameManager.h"

TimerManager::TimerManager()
{
}

TimerManager::~TimerManager()
{
}

void TimerManager::PushTask( std::chrono::system_clock::time_point time,
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
		
		// �ð��� ���� �ʾ����� �ٽ� ����
		if ( ev.m_time > std::chrono::system_clock::now() )
		{
			m_timerQueue.push( ev );
			std::this_thread::sleep_for( 10ms );
			continue;
		}

		// �ð��� �Ǿ��ٸ� ���޹��� �̺�Ʈ�� �ǵ�����
		GameManager::GetInstance().PushTask( ev.m_task.first, ev.m_task.second );

	}
}
