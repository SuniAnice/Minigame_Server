

#include "GameManager.h"
#include "MatchMaker.h"
#include "LogUtil.h"


MatchMaker::MatchMaker() {}

MatchMaker::~MatchMaker() {}

void MatchMaker::ThreadFunc()
{
	std::pair <Match::ETaskType, void* > task;
	while ( true )
	{
		if ( !m_tasks.try_pop( task ) )
		{
			std::this_thread::sleep_for( 10ms );
			continue;
		}
		
		switch ( task.first )
		{
		case Match::ETaskType::StartMatching:
		{
			Match::StartMatchingTask* t = reinterpret_cast< Match::StartMatchingTask* >( task.second );
			if ( t->m_session != nullptr )
			{
				m_matchingUser.emplace_back( t->m_session );
				t->m_session->m_isMatching = true;
				// 최대 인원수만큼의 플레이어가 존재한다면
				if ( m_matchingUser.size() >= MAX_PLAYER_IN_ROOM )
				{
					GameRoom* room = new GameRoom;
					for ( int i = 0; i < MAX_PLAYER_IN_ROOM; i++ )
					{
						if ( m_matchingUser.front() != nullptr )
						{
							room->m_userSessions.push_back( m_matchingUser.front() );
							room->m_userInfo[ m_matchingUser.front()->m_key ].m_userNum = m_matchingUser.front()->m_key;
							wmemcpy( room->m_userInfo[ m_matchingUser.front()->m_key ].m_nickname,
								m_matchingUser.front()->m_nickname.c_str(), m_matchingUser.front()->m_nickname.size() );
							m_matchingUser.front()->m_isMatching = false;
						}
						m_matchingUser.pop_front();
					}
					GameManager::GetInstance().PushTask( INGAME::ETaskType::RoomCreate, new INGAME::CreateRoomTask{ room } );
				}
				PRINT_LOG( t->m_session->m_key + "번 플레이어 매칭 시작" );
				delete task.second;
			}
		}
			break;
		case Match::ETaskType::StopMatching:
		{
			Match::StopMatchingTask* t = reinterpret_cast< Match::StopMatchingTask* >( task.second );
			if ( t->m_session != nullptr )
			{
				t->m_session->m_isMatching = false;
				m_matchingUser.remove( t->m_session );
				PRINT_LOG( t->m_session->m_key + "번 플레이어 매칭 취소" );
				delete task.second;
			}
		}
		break;
		case Match::ETaskType::UserRemove:
		{
			Match::RemovePlayerTask* t = reinterpret_cast<Match::RemovePlayerTask*>( task.second );
			if ( t->m_session != nullptr )
			{
				if ( std::find( m_matchingUser.begin(), m_matchingUser.end(), t->m_session ) != m_matchingUser.end() )
				{
					// 큐에 있다면 삭제
					m_matchingUser.remove( t->m_session );
					delete ( t->m_session );
					PRINT_LOG( "플레이어 매칭중 퇴장" );
				}
				else
				{
					// 아니면 게임매니저로 위임
					GameManager::GetInstance().PushTask( INGAME::ETaskType::RemovePlayer, 
						new INGAME::RemovePlayerTask{ t->m_session->m_roomIndex, t->m_session->m_key, t->m_session } );

				}
				delete task.second;
			}
		}
		break;
		}
	}
}
