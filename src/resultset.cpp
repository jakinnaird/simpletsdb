/*
 * Simple Time-Series Database
 *
 * Result Set
 *
 */

#include "datastore.hpp"
#include "resultset.hpp"

#include <ctime>

ResultSet::ResultSet(sqlite3_stmt *query)
	: m_Query(query)
{
}

ResultSet::~ResultSet(void)
{
	if (m_Query)
	{
		sqlite3_finalize(m_Query);
		m_Query = nullptr;
	}
}

bool ResultSet::Execute(uint64_t startTime, uint64_t endTime,
	std::vector<dps> &results)
{
	if (m_Query == nullptr)
		return false;

	sqlite3_bind_int64(m_Query, 1, startTime);
	sqlite3_bind_int64(m_Query, 2, endTime);

	int result = sqlite3_step(m_Query);
	while (result == SQLITE_ROW)
	{
		dps ret;
		ret.timestamp = sqlite3_column_int64(m_Query, 0);
		ret.value = sqlite3_column_double(m_Query, 1);
		results.push_back(ret);

		result = sqlite3_step(m_Query);
	}

	sqlite3_reset(m_Query);	// reset the query
	return true;
}
