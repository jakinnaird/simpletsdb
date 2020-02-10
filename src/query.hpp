/*
 * Simple Time-Series Database
 *
 * Query
 *
 */

#pragma once

#include <string>

class Query
{
private:
	std::string m_Query;
	std::string m_Metric;
	std::string m_Downsampler;

public:
	Query(const std::string &query);
	~Query(void);

	const std::string& GetMetric(void) const { return m_Metric; }
	const std::string& GetQuery(void) const { return m_Query; }
	const std::string& GetDownsampler(void) const { return m_Downsampler; }
};
