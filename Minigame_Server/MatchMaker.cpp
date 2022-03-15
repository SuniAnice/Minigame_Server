

#include "MatchMaker.h"


MatchMaker::MatchMaker()
{
	m_Thread = static_cast<std::thread> ( [&]()
		{
			this->ThreadFunc();
		} );
}

MatchMaker::~MatchMaker()
{
	m_Thread.join();
}

void MatchMaker::PushTask( MATCH::TASK_TYPE type, void* info )
{
}

void MatchMaker::ThreadFunc()
{
	std::pair <MATCH::TASK_TYPE, void* > task;
	while ( true )
	{
		if ( !m_tasks.try_pop( task ) )
		{
			std::this_thread::yield();
			continue;
		}

		switch ( task.first )
		{
		}
	}
}
