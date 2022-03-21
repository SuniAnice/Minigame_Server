

#include "AutoCall.hpp"
#include "DBManager.h"
#include <sqlext.h>


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
						DB::LoadTask* t = reinterpret_cast<DB::LoadTask*>( task.second );
						if ( t->m_Session != nullptr )
						{
							Base::AutoCall defer( [&t]() { delete t; } );
						}
					}
					break;
					case DB::ETaskType::SaveInfo:
					{
						DB::SaveTask* t = reinterpret_cast<DB::SaveTask*>( task.second );
						if ( t->m_Session != nullptr )
						{
							Base::AutoCall defer( [&t]() { delete t; } );
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
