/*
 * Simple Time-Series Database
 *
 * Result Set
 *
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sqlite3.h"

class ResultSet
{
public:
	struct dps
	{
		uint64_t timestamp;
		double value;
	};

private:
	sqlite3_stmt *m_Query;

public:
	ResultSet(sqlite3_stmt *query);
	~ResultSet(void);

	bool Execute(uint64_t startTime, uint64_t endTime,
		std::vector<dps> &results);
};
