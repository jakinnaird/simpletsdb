/*
 * Simple Time-Series Database
 *
 * Downsampler
 *
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "resultset.hpp"

class Downsampler
{
private:
	enum Method
	{
		NONE,
		AVG,
		SUM,
		MIN,
		MAX
	} m_Method;

	uint64_t m_Interval;

public:
	Downsampler(const std::string &type);
	~Downsampler(void);

	std::size_t Decimate(const std::vector<ResultSet::dps> &results,
		std::vector<ResultSet::dps> &output);

private:
	double Avg(const std::vector<ResultSet::dps>::const_iterator &start,
		const std::vector<ResultSet::dps>::const_iterator &end);
	double Sum(const std::vector<ResultSet::dps>::const_iterator &start,
		const std::vector<ResultSet::dps>::const_iterator &end);
	double Min(const std::vector<ResultSet::dps>::const_iterator &start,
		const std::vector<ResultSet::dps>::const_iterator &end);
	double Max(const std::vector<ResultSet::dps>::const_iterator &start,
		const std::vector<ResultSet::dps>::const_iterator &end);
};
