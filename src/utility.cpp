/*
 * Simple Time-Series Database
 *
 * Utility routines
 *
 */

#include "utility.hpp"

uint64_t ParseTime(const std::string &str)
{
	char *end = nullptr;
	uint64_t _time = strtoull(str.c_str(), &end, 10);
	if (end && *end)
	{
		// this isn't a full timestamp, so process the string
		std::string rel(end);

		// make sure that it's '-ago'
		if (rel.find("-ago") == std::string::npos)
			return 0;	// bad timestamp, return everything?

		// determine if it's minutes, hours, or days
		if (rel[0] == 'm')
			_time *= 60;
		else if (rel[0] == 'h')
			_time *= 60 * 60;
		else if (rel[0] == 'd')
			_time *= 60 * 60 * 24;
	}

	return _time;
}

std::size_t SplitString(const std::string &input, char delim,
	std::vector<std::string> &output)
{
	std::string::size_type start = 0;
	std::string::size_type pos = input.find(delim, start);
	while (pos != std::string::npos)
	{
		output.push_back(input.substr(start, pos - start));

		start = pos + 1;

		pos = input.find(delim, start);
	}

	if (start < input.length())
		output.push_back(input.substr(start));

	return output.size();
}
