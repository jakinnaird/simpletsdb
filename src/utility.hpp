/*
 * Simple Time-Series Database
 *
 * Utility routines
 *
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

uint64_t ParseTime(const std::string &str);
std::size_t SplitString(const std::string &input, char delim,
	std::vector<std::string> &output);
