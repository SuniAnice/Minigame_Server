

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
				t->session->isMatching = true;
				// �ִ� �ο�����ŭ�� �÷��̾ �����Ѵٸ�
				if ( m_matchingUser.size() >= MAX_PLAYER_IN_ROOM )
				{
					GameRoom* room = new GameRoom;
					for ( int i = 0; i < MAX_PLAYER_IN_ROOM; i++ )
					{
						if ( m_matchingUser.front() != nullptr )
						{
							room->userSessions.push_back( m_matchingUser.front() );
							room->userInfo[ m_matchingUser.front()->key ].userNum = m_matchingUser.front()->key;
							wmemcpy( room->userInfo[ m_matchingUser.front()->key ].nickname,
								m_matchingUser.front()->nickname.c_str(), m_matchingUser.front()->nickname.size() );
						}
						m_matchingUser.pop_front();
					}
					t->session->isMatching = false;
					GameManager::GetInstance().PushTask( INGAME::TASK_TYPE::ROOM_CREATE, new INGAME::CreateRoomTask{ room } );
				}
				PRINT_LOG( t->session->key + "�� �÷��̾� ��Ī ����" );
				delete task.second;
			}
		}
			break;
		case MATCH::TASK_TYPE::USER_STOPMATCHING:
		{
			MATCH::StopMatchingTask* t = reinterpret_cast< MATCH::StopMatchingTask* >( task.second );
			if ( t != nullptr )
			{
				t->session->isMatching = false;
				m_matchingUser.remove( t->session );
				PRINT_LOG( t->session->key + "�� �÷��̾� ��Ī ���" );
				delete task.second;
			}
		}
		break;
		case MATCH::TASK_TYPE::USER_REMOVE:
		{
			MATCH::RemovePlayerTask* t = reinterpret_cast<MATCH::RemovePlayerTask*>( task.second );
			if ( t != nullptr )
			{
				if ( std::find( m_matchingUser.begin(), m_matchingUser.end(), t->session ) != m_matchingUser.end() )
				{
					// ť�� �ִٸ� ����
					m_matchingUser.remove( t->session );
					delete ( t->session );
					PRINT_LOG( "�÷��̾� ��Ī�� ����" );
				}
				else
				{
					// �ƴϸ� ���ӸŴ����� ����
					GameManager::GetInstance().PushTask( INGAME::TASK_TYPE::REMOVE_PLAYER, new INGAME::RemovePlayerTask{ t->session->roomIndex, t->session->key, t->session } );

				}
				delete task.second;
			}
		}
		break;
		}
	}
}
