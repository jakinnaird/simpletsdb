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

	std::string m_Metric;
	std::string m_Downsampler;

public:
	ResultSet(sqlite3_stmt *query,
		const std::string &metric,
		const std::string &downsampler);
	~ResultSet(void);

	bool Execute(uint64_t startTime, uint64_t endTime,
		std::vector<dps> &results);

	const std::string& GetMetric(void) const { return m_Metric; }
	const std::string& GetDownsampler(void) const { return m_Downsampler; }
};
