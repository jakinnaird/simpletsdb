/*
 * Simple Time-Series Database
 *
 * Metric
 *
 */

#pragma once

#include <cstdint>
#include <string>

class Metric
{
private:
	std::string m_Name;
	uint64_t m_Timestamp;
	double m_Value;
	std::string m_Tags;

	bool m_IsOk;
	std::string m_Error;

public:
	Metric(void);
	Metric(const std::string &line);
	Metric(const std::string &name,
		uint64_t &timestamp, double &value,
		const std::string &tags);
	Metric(const Metric &that);
	~Metric(void);

	void operator = (const Metric &that);

	bool IsValid(std::string &error);

	const std::string& Name(void) const { return m_Name; }
	const uint64_t& Timestamp(void) const { return m_Timestamp; }
	const double& Value(void) const { return m_Value; }
	const std::string &Tags(void) const { return m_Tags; }
};
