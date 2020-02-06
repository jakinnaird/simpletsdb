/*
 * Simple Time-Series Database
 *
 * Network processor
 *
 */

#pragma once

#include <cstdint>
#include <string>

#include "datastore.hpp"
#include "metric.hpp"
#include "stats.hpp"

class TelnetProcessor;
class HttpProcessor;

class NetworkProcessor
{
private:
	std::string m_BindAddr;

	Datastore *m_DataStore;
	Statistics *m_Stats;

	TelnetProcessor *m_Telnet;
	HttpProcessor *m_Http;

public:
	NetworkProcessor(const std::string &bindAddr,
		Datastore *datastore, Statistics *stats);
	~NetworkProcessor(void);

	bool StartTelnetInterface(const std::string &port,
		uint32_t backlog = 10);
	bool StartHTTPInterface(const std::string &port);

	void StopTelnetInterface(void);
	void StopHTTPInterface(void);
};
