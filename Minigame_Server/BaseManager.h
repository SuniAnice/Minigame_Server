

#pragma once

#include "Slngleton.h"
#include "global.h"
#include <thread>
#include <concurrent_queue.h>


template <typename T>
class BaseManager
{
public:
	BaseManager()
	{
		m_Thread = static_cast< std::thread > ( [&]()
			{
				this->ThreadFunc();
			} );
	}
	virtual ~BaseManager()
	{
		m_Thread.join();
	}

	void PushTask( T type, void* info )
	{
		m_tasks.push( { type, info } );

	}

	virtual void ThreadFunc() = 0;
protected:
	std::thread m_Thread;

	concurrency::concurrent_queue< std::pair < T, void* > > m_tasks;
};