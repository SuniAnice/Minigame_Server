

#include "GameManager.h"
#include "MatchMaker.h"


MatchMaker::MatchMaker() {}


MatchMaker::~MatchMaker() {}

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
		case MATCH::TASK_TYPE::USER_STARTMATCHING:
		{
			MATCH::StartMatchingTask* t = reinterpret_cast<MATCH::StartMatchingTask*>( task.second );
			if ( t != nullptr )
			{
				m_matchingUser.emplace_back( t->session );
				if ( m_matchingUser.size() >= MAX_PLAYER_IN_ROOM )
				{
					GameRoom* room = new GameRoom;
					for ( int i = 0; i < MAX_PLAYER_IN_ROOM; i++ )
					{
						room->users.push_back( m_matchingUser.front() );
						m_matchingUser.pop_front();
					}
					GameManager::GetInstance().PushTask( INGAME::TASK_TYPE::ROOM_CREATE, new INGAME::CreateRoomTask{ room } );
				}
				delete task.second;
			}
		}
			break;
		case MATCH::TASK_TYPE::USER_STOPMATCHING:
		{
			MATCH::StopMatchingTask* t = reinterpret_cast< MATCH::StopMatchingTask* >( task.second );
			if ( t != nullptr )
			{
				m_matchingUser.remove( t->session );
				delete task.second;
			}
		}
		break;
		}
	}
}
