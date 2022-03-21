

#pragma once


#include "BaseManager.h"


class DBManager : public Base::TSingleton < DBManager >,
	public BaseManager < DB::ETaskType >
{
public:
	DBManager();
	virtual ~DBManager();

	virtual void ThreadFunc();
};

