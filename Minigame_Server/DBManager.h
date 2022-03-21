

#pragma once


#include "BaseManager.h"
#include <sqlext.h>


class DBManager : public Base::TSingleton < DBManager >,
	public BaseManager < DB::ETaskType >
{
public:
	DBManager();
	virtual ~DBManager();

	virtual void ThreadFunc();

private:
	void _HandleDiagnosticRecord( SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode );
};

