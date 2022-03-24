

#include "AutoCall.hpp"
#include "DBManager.h"
#include "LobbyManager.h"
#include "LogUtil.h"


DBManager::DBManager() {}

DBManager::~DBManager() {}

void DBManager::ThreadFunc()
{
	std::pair <DB::ETaskType, void* > task;
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode;

	// Allocate environment handle  
	retcode = SQLAllocHandle( SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv );

	// Set the ODBC version environment attribute  
	if ( retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO ) {
		retcode = SQLSetEnvAttr( henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0 );

		// Allocate connection handle  
		if ( retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO ) {
			retcode = SQLAllocHandle( SQL_HANDLE_DBC, henv, &hdbc );

			// Set login timeout to 5 seconds  
			if ( retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO ) {
				SQLSetConnectAttr( hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0 );

				// Connect to data source  
				retcode = SQLConnect( hdbc, (SQLWCHAR*)L"minigame_server", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0 );

				while ( true )
				{
					if ( !m_tasks.try_pop( task ) )
					{
						std::this_thread::sleep_for( 10ms );
						continue;
					}

					switch ( task.first )
					{
					case DB::ETaskType::LoadInfo:
					{
						DB::LoadTask* t = reinterpret_cast< DB::LoadTask* >( task.second );
						if ( t->m_Session != nullptr )
						{
							retcode = SQLAllocHandle( SQL_HANDLE_STMT, hdbc, &hstmt );
							Base::AutoCall defer( [&t]() { delete t; } );
							SQLWCHAR buf[ 255 ];
							wsprintf( buf, L"EXEC READ_PLAYERINFO %s", t->m_nickname.c_str() );
							retcode = SQLExecDirect( hstmt, (SQLWCHAR*)buf, SQL_NTS );
							SQLLEN cbTotalScore = 0;
							SQLINTEGER totalScore = 0;
							if ( retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO ) {

								retcode = SQLBindCol( hstmt, 1, SQL_C_ULONG, &totalScore, 10, &cbTotalScore );

								Lobby::DBInfoLoadedTask newt;

								for ( int i = 0; ; i++ ) {
									retcode = SQLFetch( hstmt );
									if ( retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO ) {
										_HandleDiagnosticRecord( hstmt, SQL_HANDLE_STMT, retcode );
									}
									if ( retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO )
									{
										// 태스크에 읽어온 변수 할당하기
										newt.m_session = t->m_Session;
										wmemcpy( newt.m_nickname, t->m_nickname.c_str(), t->m_nickname.size() );
										newt.m_score = totalScore;
									}
									else
									{
										if ( i != 0 )
											break; // end of data
										else
										{
											// db에 정보 없음, 가입 처리
											newt.m_session = t->m_Session;
											wmemcpy( newt.m_nickname, t->m_nickname.c_str(), t->m_nickname.size() );
											newt.m_score = 0;
											break;
										}
									}
								}

								SQLCancel( hstmt );
								SQLFreeHandle( SQL_HANDLE_STMT, hstmt );

								if ( newt.m_session != nullptr )
								{
									PRINT_LOG( "유저 정보 DB로부터 읽어옴" );
									LobbyManager::GetInstance().PushTask( Lobby::ETaskType::DBInfoLoaded, &newt );
								}
							}

						}
					}
					break;
					case DB::ETaskType::SaveInfo:
					{
						DB::SaveTask* t = reinterpret_cast< DB::SaveTask* >( task.second );
						if ( t->m_Session != nullptr )
						{
							Base::AutoCall defer( [&t]() { delete t; } );

							retcode = SQLAllocHandle( SQL_HANDLE_STMT, hdbc, &hstmt );
							SQLWCHAR buf[ 255 ];
							wsprintf( buf, L"EXEC SAVE_PLAYERINFO %s, %d", t->m_Session->m_nickname.c_str(), t->m_score );
							retcode = SQLExecDirect( hstmt, (SQLWCHAR*)buf, SQL_NTS );
							if ( retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO ) {
								_HandleDiagnosticRecord( hstmt, SQL_HANDLE_STMT, retcode );
							}
							// Process data  
							if ( retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO ) {
								SQLCancel( hstmt );
								SQLFreeHandle( SQL_HANDLE_STMT, hstmt );
							}
							PRINT_LOG( "유저 정보 DB에 저장" );
						}
					}
					break;
					}
				}

				SQLDisconnect( hdbc );
				SQLFreeHandle( SQL_HANDLE_DBC, hdbc );
				SQLFreeHandle( SQL_HANDLE_ENV, henv );
			}
		}
	}
	
}

void DBManager::_HandleDiagnosticRecord( SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode )
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszMessage[ 1000 ];
	WCHAR wszState[ SQL_SQLSTATE_SIZE + 1 ];
	if ( RetCode == SQL_INVALID_HANDLE ) {
		fwprintf( stderr, L"Invalid handle!\n" );
		return;
	}
	while ( SQLGetDiagRec( hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)( sizeof( wszMessage ) / sizeof( WCHAR ) ), (SQLSMALLINT*)NULL ) == SQL_SUCCESS ) {
		// Hide data truncated..
		if ( wcsncmp( wszState, L"01004", 5 ) ) {
			fwprintf( stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError );
		}
	}
}