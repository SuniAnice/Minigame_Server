

#include "GameManager.h"
#include "MatchMaker.h"
#include "LogUtil.h"


MatchMaker::MatchMaker() {}

MatchMaker::~MatchMaker() {}

void MatchMaker::ThreadFunc()
{
	std::pair <MATCH::TASK_TYPE, void* > task;
	while ( true )
	{
		if ( !m_tasks.try_pop( task ) )
		{
			std::this_thread::sleep_for( 10ms );
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
				// 접속 종료한 플레이어 체크
				for ( auto it = m_matchingUser.begin(); it != m_matchingUser.end(); it++ )
				{
					if ( *it == nullptr )
					{
						it = m_matchingUser.erase( it );
					}
				}
				// 최대 인원수만큼의 플레이어가 존재한다면
				if ( m_matchingUser.size() >= MAX_PLAYER_IN_ROOM )
				{
					GameRoom* room = new GameRoom;
					for ( int i = 0; i < MAX_PLAYER_IN_ROOM; i++ )
					{
						if ( m_matchingUser.front() != nullptr )
						{
							room->userSessions.push_back( m_matchingUser.front() );
							room->userInfo[ i ].userNum = m_matchingUser.front()->key;
							wmemcpy( room->userInfo[ i ].nickname, m_matchingUser.front()->nickname.c_str(), m_matchingUser.front()->nickname.size() );
						}
						m_matchingUser.pop_front();
					}
					GameManager::GetInstance().PushTask( INGAME::TASK_TYPE::ROOM_CREATE, new INGAME::CreateRoomTask{ room } );
				}
				PRINT_LOG( t->session->key + "번 플레이어 매칭 시작" );
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
				PRINT_LOG( t->session->key + "번 플레이어 매칭 취소" );
				delete task.second;
			}
		}
		break;
		}
	}
}
