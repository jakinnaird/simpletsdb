/*
 * Simple Time-Series Database
 *
 * Metric
 *
 */

#include <sstream>

#include "metric.hpp"

Metric::Metric(void)
	: m_Timestamp(0), m_Value(0)
{
	m_IsOk = false;
}

Metric::Metric(const std::string &line)
	: m_Timestamp(0), m_Value(0), m_IsOk(false)
{
	// parse the line
	std::string::size_type start = 0;
	std::string::size_type pos = line.find(' ', start);
	if (pos != std::string::npos)
		m_Name = line.substr(start, pos);

	start = pos + 1;
	pos = line.find(' ', pos + 1);
	if (pos != std::string::npos)
	{
		std::string _timestamp = line.substr(start, pos - start);
		char *end = nullptr;
		m_Timestamp = strtoull(_timestamp.c_str(), &end, 10);
		if (end && *end)
		{
			std::ostringstream err;
			err << "Invalid timestamp format: " << m_Timestamp << ", '" << end << "'";
			m_Error.assign(err.str());
			return;
		}
	}

	start = pos + 1;
	pos = line.find(' ', start);
	if (pos != std::string::npos)
	{
		std::string _value = line.substr(start, pos - start);
		char *end = nullptr;
		m_Value = strtod(_value.c_str(), &end);
		if (end && *end)
		{
			std::ostringstream err;
			err << "Invalid value: " << m_Value << ", '" << end << "'";
			m_Error.assign(err.str());
			return;
		}
	}

	start = pos + 1;
	m_Tags = line.substr(start);

	if (m_Name.length() > 0 && m_Tags.length() > 0)
		m_IsOk = true;
}

Metric::Metric(const std::string &name,
	uint64_t &timestamp, double &value,
	const std::string &tags)
	: m_Timestamp(timestamp), m_Value(value), m_IsOk(false)
{
	if (name.length() == 0 || tags.length() == 0)
		return;	// bad metric

	m_Name.assign(name);
	m_Tags.assign(tags);

	m_IsOk = true;
}

Metric::Metric(const Metric &that)
{
	m_Name.assign(that.m_Name);
	m_Timestamp = that.m_Timestamp;
	m_Value = that.m_Value;
	m_Tags.assign(that.m_Tags);

	m_IsOk = that.m_IsOk;
	m_Error.assign(that.m_Error);
}

Metric::~Metric(void)
{
}

void Metric::operator= (const Metric &that)
{
	m_Name.assign(that.m_Name);
	m_Timestamp = that.m_Timestamp;
	m_Value = that.m_Value;
	m_Tags.assign(that.m_Tags);

	m_IsOk = that.m_IsOk;
	m_Error.assign(that.m_Error);
}

bool Metric::IsValid(std::string &error)
{
	if (!m_IsOk)
		error.assign(m_Error);

	return m_IsOk;
}
