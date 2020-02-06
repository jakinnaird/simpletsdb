/*
 * Simple Time-Series Database
 *
 * Scheduling Kernel
 *
 */

#include "kernel.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "sqlite3.h"
#include "tclap/CmdLine.h"

#if defined(_WIN32) || defined(WIN32)
#define PATH_SEP	"\\"
#else
#define PATH_SEP	"/"
#endif

#if defined(_DEBUG) || defined(DEBUG)
#define DEFAULT_LOG_LEVEL	"debug"
#else
#define DEFAULT_LOG_LEVEL	"info"
#endif

void sqliteErrorCallback(void *arg, int32_t code, const char *msg)
{
	spdlog::warn("sqlite error: {0} ({1})", msg, code);
}

Kernel::Kernel(std::vector<std::string> &args,
	const std::string &logdir, const std::string &datadir,
	const std::string &hostname)
{
	m_Config = nullptr;
	m_Stats = nullptr;
	m_Net = nullptr;
	m_DataStore = nullptr;

	try
	{
		std::string defaultConf(datadir);
		defaultConf.append(PATH_SEP);
		defaultConf.append("stsdbd.conf");

		TCLAP::CmdLine params("");
		TCLAP::ValueArg<std::string> configArg("", "config", "unused",
			false, defaultConf, "unused");

		params.add(configArg);

		params.parse(args);
		if (configArg.isSet() && configArg.getValue() != ".")
			m_Config = new INIReader(configArg.getValue());
		else
			m_Config = new INIReader(defaultConf);
	}
	catch (...)
	{
	}

	if (m_Config == nullptr)
		throw std::runtime_error("Failed to create configuration object");

	// load the default configuration
	m_LogDir = m_Config->Get("stsdbd", "logpath", logdir);
	m_DataDir = m_Config->Get("stsdbd", "datapath", datadir);

	// prepare the log sinks
	spdlog::flush_every(std::chrono::seconds(3));	// flush every 3 seconds
	std::vector<spdlog::sink_ptr> sinks;
	std::ostringstream log_file;
	log_file << m_LogDir << PATH_SEP << "simpletsdb.log";
	sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file.str()));

	spdlog::set_default_logger(
		std::make_shared<spdlog::logger>("simpleTSDB", std::begin(sinks), std::end(sinks)));

	// configure the logger
	std::string loglvl = m_Config->Get("stsdbd", "loglevel", DEFAULT_LOG_LEVEL);
	if (loglvl == "crit")
		spdlog::set_level(spdlog::level::critical);
	else if (loglvl == "error")
		spdlog::set_level(spdlog::level::err);
	else if (loglvl == "warn")
		spdlog::set_level(spdlog::level::warn);
	else if (loglvl == "debug")
		spdlog::set_level(spdlog::level::debug);
	else // info, or unknown
		spdlog::set_level(spdlog::level::info);

	sqlite3_config(SQLITE_CONFIG_LOG, sqliteErrorCallback, nullptr);
	spdlog::info("SQLite3 version: {0}", sqlite3_libversion());

	// create the statistics system
	m_Stats = new Statistics();
	if (m_Stats == nullptr)
		throw std::runtime_error("Failed to create statistics monitor");

	// create the datastore
	m_DataStore = new Datastore(m_DataDir, 
		m_Config->Get("stsdbd", "dbext", "tsdb"),
		m_Config->Get("stsdb", "hostname", hostname), m_Stats);
	if (m_DataStore == nullptr)
		throw std::runtime_error("Failed to create datastore");

	// create the network processor
	m_Net = new NetworkProcessor(m_Config->Get("stsdbd", "bind_address", "127.0.0.1"),
		m_DataStore, m_Stats);
	if (m_Net == nullptr)
		throw std::runtime_error("Failed to create network processor");
}

Kernel::~Kernel(void)
{
	Stop();

	delete m_Net;
	delete m_DataStore;
	delete m_Stats;

	spdlog::info("SimpleTSDB stopped");

	spdlog::shutdown();
}

void Kernel::Start(void)
{
	if (!m_Stats->StartThread())
		throw std::runtime_error("Failed to start statistics");

	if (!m_DataStore->StartThread())
		throw std::runtime_error("Failed to start datastore");

	if (!m_Net->StartTelnetInterface(m_Config->Get("stsdbd", "telnet_port", "2181")))
		throw std::runtime_error("Failed to start telnet interface");
	if (!m_Net->StartHTTPInterface(m_Config->Get("stsdbd", "http_port", "8080")))
		throw std::runtime_error("Failed to start HTTP interface");

	spdlog::info("SimpleTSDB started");
}

void Kernel::Stop(void)
{
	m_Net->StopHTTPInterface();
	m_Net->StopTelnetInterface();
	m_DataStore->StopThread();
	m_Stats->StopThread();
}
