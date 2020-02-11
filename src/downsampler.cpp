/*
 * Simple Time-Series Database
 *
 * Downsampler
 *
 */

#include "downsampler.hpp"
#include "utility.hpp"

#include <algorithm>

#include "spdlog/spdlog.h"

Downsampler::Downsampler(const std::string &type)
	: m_Method(Method::NONE), m_Interval(0)
{
	if (type.length() > 0)
	{
		std::vector<std::string> parts;
		SplitString(type, '-', parts);
		if (parts.size() != 2)
			spdlog::warn("Invalid downsampler: {0}", type.c_str());
		else
		{
			// convert the interval
			char *end = nullptr;
			m_Interval = strtoull(parts[0].c_str(), &end, 10);
			if (end && *end)
			{
				std::string rel(end);

				if (rel.compare("all") == 0)
					m_Interval = 0;
				else if (rel[0] == 'm')
					m_Interval *= 60;
				else if (rel[0] == 'h')
					m_Interval *= 60 * 60;
				else if (rel[0] == 'd')
					m_Interval *= 60 * 60 * 24;
			}

			// convert the aggregator
			if (parts[1] == "avg")
				m_Method = Method::AVG;
			else if (parts[1] == "sum")
				m_Method = Method::SUM;
			else if (parts[1] == "min")
				m_Method = Method::MIN;
			else if (parts[1] == "max")
				m_Method = Method::MAX;
		}
	}
}

Downsampler::~Downsampler(void)
{
}

std::size_t Downsampler::Decimate(const std::vector<ResultSet::dps> &results,
	std::vector<ResultSet::dps> &output)
{
	if (m_Method == Method::NONE)
	{
		output = results;
	}
	else
	{
		if (m_Interval == 0)
		{
			double result = 0;

			// resolve everything based on the aggregation method
			if (m_Method == Method::AVG)
				result = Avg(results.begin(), results.end());
			else if (m_Method == Method::SUM)
				result = Sum(results.begin(), results.end());
			else if (m_Method == Method::MIN)
				result = Min(results.begin(), results.end());
			else if (m_Method == Method::MAX)
				result = Max(results.begin(), results.end());

			ResultSet::dps dps;
			dps.timestamp = results.begin()->timestamp;
			dps.value = result;
			output.push_back(dps);
		}
		else
		{
			std::vector<ResultSet::dps>::const_iterator next;
			for (std::vector<ResultSet::dps>::const_iterator start = results.begin();
				start != results.end();)
			{
				uint64_t timestamp = start->timestamp;
				uint64_t interval = 0;
				next = start;

				while (next != results.end())
				{
					interval = next->timestamp - timestamp;
					if (interval < m_Interval)
						next = std::next(next, 1);
					else
					{
						// over the interval, so aggregate
						double result = 0;

						// resolve everything based on the aggregation method
						if (m_Method == Method::AVG)
							result = Avg(start, next);
						else if (m_Method == Method::SUM)
							result = Sum(start, next);
						else if (m_Method == Method::MIN)
							result = Min(start, next);
						else if (m_Method == Method::MAX)
							result = Max(start, next);

						ResultSet::dps dps;
						dps.timestamp = start->timestamp;
						dps.value = result;
						output.push_back(dps);
						break;
					}
				}

				if (next == results.end())
				{
					// over the interval, so aggregate
					double result = 0;

					// resolve everything based on the aggregation method
					if (m_Method == Method::AVG)
						result = Avg(start, next);
					else if (m_Method == Method::SUM)
						result = Sum(start, next);
					else if (m_Method == Method::MIN)
						result = Min(start, next);
					else if (m_Method == Method::MAX)
						result = Max(start, next);

					ResultSet::dps dps;
					dps.timestamp = start->timestamp;
					dps.value = result;
					output.push_back(dps);
				}

				start = next;
			}
		}
	}

	return output.size();
}

double Downsampler::Avg(const std::vector<ResultSet::dps>::const_iterator &start,
	const std::vector<ResultSet::dps>::const_iterator &end)
{
	double value = 0;
	std::size_t count = 0;

	for (std::vector<ResultSet::dps>::const_iterator dp = start;
		dp != end; ++dp)
	{
		value += (*dp).value;
		++count;
	}

	if (count > 0)
		return (value / count);
	else
		return 0;
}

double Downsampler::Sum(const std::vector<ResultSet::dps>::const_iterator &start,
	const std::vector<ResultSet::dps>::const_iterator &end)
{
	double value = 0;

	for (std::vector<ResultSet::dps>::const_iterator dp = start;
		dp != end; ++dp)
	{
		value += (*dp).value;
	}

	return value;
}

double Downsampler::Min(const std::vector<ResultSet::dps>::const_iterator &start,
	const std::vector<ResultSet::dps>::const_iterator &end)
{
	bool first = true;
	double value = 0;

	for (std::vector<ResultSet::dps>::const_iterator dp = start;
		dp != end; ++dp)
	{
		if (first)
		{
			value = (*dp).value;
			first = false;
		}
		else
		{
			value = std::min(value, (*dp).value);
		}
	}

	return value;
}

double Downsampler::Max(const std::vector<ResultSet::dps>::const_iterator &start,
	const std::vector<ResultSet::dps>::const_iterator &end)
{
	bool first = true;
	double value = 0;

	for (std::vector<ResultSet::dps>::const_iterator dp = start;
		dp != end; ++dp)
	{
		if (first)
		{
			value = (*dp).value;
			first = false;
		}
		else
		{
			value = std::max(value, (*dp).value);
		}
	}

	return value;
}
