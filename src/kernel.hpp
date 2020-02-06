/*
 * Simple Time-Series Database
 *
 * Scheduling Kernel
 *
 */

#pragma once

#include <string>
#include <vector>

#include "datastore.hpp"
#include "metric.hpp"
#include "network.hpp"
#include "stats.hpp"

#include "INIReader.h"

class Kernel
{
private:
	INIReader *m_Config;

	std::string m_LogDir;
	std::string m_DataDir;

	Statistics *m_Stats;
	Datastore *m_DataStore;
	NetworkProcessor *m_Net;

public:
	Kernel(std::vector<std::string> &args,
		const std::string &logdir, const std::string &datadir,
		const std::string &hostname);
	~Kernel(void);

	void Start(void);
	void Stop(void);
};
