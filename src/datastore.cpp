/*
 * Simple Time-Series Database
 *
 * Datastore
 *
 */

#include "datastore.hpp"

#include "spdlog/spdlog.h"

#if defined(_WIN32) || defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#if defined(_WIN32) || defined(WIN32)
#define PATH_SEP	"\\"
#else
#define PATH_SEP	"/"
#endif

#define BULK_COUNT	10

#define SQL_CREATE_TABLE_METRIC	\
	"CREATE TABLE METRIC (TIMESTAMP INTEGER NOT NULL, " \
	"VALUE NUMBER NOT NULL, TAGS TEXT NOT NULL);"
#define SQL_VERIFY_TABLE \
	"SELECT COUNT(name) FROM sqlite_master WHERE TYPE='table' AND NAME=?001;"
#define SQL_ENABLE_WAL \
	"PRAGMA journal_mode=WAL;"

#define SQL_INSERT_METRIC \
	"INSERT INTO METRIC (TIMESTAMP, VALUE, TAGS) VALUES (?001, ?002, ?003);"

Datastore::Datastore(const std::string &dataDir,
	const std::string &dbExt, const std::string &hostname, Statistics *stats)
	: m_DataDir(dataDir), m_DbExt(dbExt), m_Hostname(hostname), m_Stats(stats)
{
	m_QueueSize = 0;
	m_Running = false;

	m_Thread = new Thread(this);
	if (m_Thread == nullptr)
		throw std::runtime_error("Failed to create datastore thread");
}

Datastore::~Datastore(void)
{
	delete m_Thread;
}

bool Datastore::StartThread(void)
{
	return m_Thread->Start();
}

void Datastore::StopThread(void)
{
	m_Thread->Stop();
}

void Datastore::QueueMetric(const Metric &m)
{
	m_MetricQueue.enqueue(m);
	m_QueueSize += 1;
}

bool Datastore::CacheDatabase(const std::string &name, const std::string &path)
{
	dbconn *conn = new dbconn;
	conn->db = nullptr;
	conn->insert = nullptr;

	// check to make sure that this hasn't already been loaded
	if (m_Store.find(name) != m_Store.end())
	{
		spdlog::warn("Database {0} already loaded", path.c_str());
		return true;	// okay, I guess
	}

	// try to open the database
	int result = sqlite3_open_v2(path.c_str(), &conn->db, 
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, nullptr);
	if (result != SQLITE_OK)
	{
		spdlog::warn(sqlite3_errstr(result));
		delete conn;
		return false;
	}

	// validate the schema
	sqlite3_stmt *stmt = nullptr;
	result = sqlite3_prepare_v2(conn->db, SQL_VERIFY_TABLE, -1, &stmt, nullptr);
	if (result != SQLITE_OK)
	{
		spdlog::warn(sqlite3_errstr(result));
		sqlite3_close_v2(conn->db);
		delete conn;
		return false;
	}

	sqlite3_bind_text(stmt, 1, "METRIC", -1, nullptr);

	result = sqlite3_step(stmt);
	if (result != SQLITE_ROW)
	{
		// the table doesn't exist, or there is an error
		spdlog::warn(sqlite3_errstr(result));
		sqlite3_finalize(stmt);
		sqlite3_close_v2(conn->db);
		delete conn;
		return false;
	}

	// we have a result, check it
	result = sqlite3_column_int(stmt, 0);
	if (result == 0)
	{
		// the table doesn't exist, so this isn't a properly formed database
		spdlog::warn("Database {0} isn't a TSDB file, skipping", path.c_str());
		sqlite3_finalize(stmt);
		sqlite3_close_v2(conn->db);
		delete conn;
		return false;
	}

	sqlite3_finalize(stmt);

	// enable Write-Ahead-Logging
	char *error = nullptr;
	result = sqlite3_exec(conn->db, SQL_ENABLE_WAL, nullptr,
		nullptr, &error);
	if (result != SQLITE_OK)
	{
		spdlog::warn(error);
		sqlite3_free(error);
		sqlite3_close_v2(conn->db);
		delete conn;
		return false;
	}

	// create the prepared statements
	result = sqlite3_prepare_v2(conn->db, SQL_INSERT_METRIC, -1,
		&conn->insert, nullptr);
	if (result != SQLITE_OK)
	{
		spdlog::warn(sqlite3_errstr(result));
		sqlite3_close_v2(conn->db);
		delete conn;
		return false;
	}

	// store the database in the cache
	m_Store.insert(std::pair<std::string, dbconn*>(name, conn));
	return true;
}

Datastore::dbconn* Datastore::CreateDatabase(const std::string &name)
{
	// ensure that the name is valid
	if (name.length() == 0)
		return nullptr;

	dbconn *conn = new dbconn;
	conn->db = nullptr;
	conn->insert = nullptr;

	// assemble the path
	std::string path(m_DataDir);
	path.append(PATH_SEP);
	path.append(name);
	path.append(".");
	path.append(m_DbExt);

	int result = sqlite3_open_v2(path.c_str(), &conn->db,
		SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_NOMUTEX, nullptr);
	if (result != SQLITE_OK)
	{
		spdlog::warn(sqlite3_errstr(result));
		delete conn;
		return nullptr;
	}

	// create the schema
	char *error = nullptr;
	result = sqlite3_exec(conn->db, SQL_CREATE_TABLE_METRIC, nullptr,
		nullptr, &error);
	if (result != SQLITE_OK)
	{
		spdlog::warn(error);
		sqlite3_free(error);
		sqlite3_close_v2(conn->db);
		delete conn;
		return nullptr;
	}

	// enable Write-Ahead-Logging
	result = sqlite3_exec(conn->db, SQL_ENABLE_WAL, nullptr,
		nullptr, &error);
	if (result != SQLITE_OK)
	{
		spdlog::warn(error);
		sqlite3_free(error);
		sqlite3_close_v2(conn->db);
		delete conn;
		return nullptr;
	}

	// create the prepared statements
	result = sqlite3_prepare_v2(conn->db, SQL_INSERT_METRIC, -1,
		&conn->insert, nullptr);
	if (result != SQLITE_OK)
	{
		spdlog::warn(sqlite3_errstr(result));
		sqlite3_close_v2(conn->db);
		delete conn;
		return false;
	}

	return conn;
}

void Datastore::WriteMetric(dbconn *conn, const Metric &metric)
{
	sqlite3_bind_int64(conn->insert, 1, metric.Timestamp());
	sqlite3_bind_double(conn->insert, 2, metric.Value());
	sqlite3_bind_text(conn->insert, 3, metric.Tags().c_str(), -1, nullptr);

	int result = sqlite3_step(conn->insert);
	if (result != SQLITE_DONE)
	{
		spdlog::warn("Error writing metric {0}: {1}", metric.Name().c_str(),
			sqlite3_errstr(result));
	}

	sqlite3_reset(conn->insert);
}

void Datastore::Start(void)
{
	spdlog::info("Starting datastore");
	spdlog::info("Data directory: {0}", m_DataDir.c_str());

	// scan the data directory for databases to cache
#if defined(_WIN32) || defined(WIN32)
	WIN32_FIND_DATAA wfd;
	HANDLE hfind = INVALID_HANDLE_VALUE;

	std::string searchpath(m_DataDir);
	searchpath.append(PATH_SEP);
	searchpath.append("*");

	hfind = FindFirstFileA(searchpath.c_str(), &wfd);
	if (hfind == INVALID_HANDLE_VALUE)
	{
		spdlog::error("Failed to search path: {0}", searchpath.c_str());
		return;
	}

	do
	{
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// ignore the directory bits
		}
		else
		{
			std::string filename(wfd.cFileName);

			// check the extension
			std::string::size_type extLen = m_DbExt.length();
			std::string::size_type filenameLen = filename.length();
			std::string sub = filename.substr(filenameLen - extLen);
			
			// clean up the filename
			filename = filename.substr(0, filenameLen - extLen - 1);

			if (sub.compare(m_DbExt) == 0)
			{
				std::string dbPath(m_DataDir);
				dbPath.append(PATH_SEP);
				dbPath.append(filename);
				dbPath.append(".");
				dbPath.append(m_DbExt);

				spdlog::info("Caching {0} database: {1}", 
					filename.c_str(), dbPath.c_str());

				if (!CacheDatabase(filename, dbPath))
					spdlog::warn("Failed to cache database: {0}", dbPath.c_str());
			}
		}
	} while (FindNextFileA(hfind, &wfd));

	FindClose(hfind);
#else
	DIR *dir = opendir(m_DataDir.c_str());
	if (dir)
	{
		struct dirent *entry = nullptr;
		while ((entry = readdir(dir)) != NULL)
		{
			if (entry->d_type == DT_REG)
			{
				// this is a file, check the name
				std::string filename(entry->d_name);

				std::string::size_type extLen = m_DbExt.length();
				std::string::size_type filenameLen = filename.length();
				std::string sub = filename.substr(filenameLen - extLen);

				// clean up the filename
				filename = filename.substr(0, filenameLen - extLen - 1);

				if (sub.compare(ext) == 0)
				{
					std::string dbPath(m_DataDir);
					dbPath.append(PATH_SEP);
					dbPath.append(filename);
					dbPath.append(".");
					dbPath.append(m_DbExt);

					spdlog::info("Caching {0} database: {1}", 
						filename.c_str(), dbPath.c_str());

					if (!CacheDatabase(filename, dbPath))
						spdlog::warn("Failed to cache database: {0}", dbPath.c_str());
				}
			}
		}
	}
	else
		return false;
#endif

	m_Running = true;
	spdlog::info("Datastore started");
}

void Datastore::Process(void)
{
	if (!m_Running)
	{
		m_Thread->Stop();
		return;
	}
	
	m_Stats->SetQueueBacklog(m_QueueSize);

	if (m_QueueSize == 0)
		this->Sleep(50);	// wait for the queue to fill back up

	// try to dequeue some metrics
	Metric m[BULK_COUNT];
	std::size_t count = 0;
	while ((count = m_MetricQueue.try_dequeue_bulk(m, BULK_COUNT)) > 0)
	{
		for (std::size_t i = 0; i < count; i++)
		{
			// find the database in the cache
			datastore_t::iterator store = m_Store.find(m[i].Name());
			if (store != m_Store.end())
				WriteMetric(store->second, m[i]);
			else
			{
				// create the database
				dbconn *conn = CreateDatabase(m[i].Name());
				if (conn)
				{
					m_Store.insert(std::pair<std::string, dbconn*>(m[i].Name(), conn));
					WriteMetric(conn, m[i]);
				}
			}
		}

		m_Stats->AddWriteCount(count);
		m_QueueSize -= count;
	}

	// write the internal statistics to the datastore if they are updated
	Statistics::Stats stats;
	bool write = m_Stats->GetStats(stats, true);
	if (write)
	{
		// get the timestamp
		uint64_t timestamp = time(nullptr);
		std::string tags("host=");
		tags.append(m_Hostname);

		// build the metrics
		std::string name("tsdb.internal.putspersecond");
		Metric pps(name, timestamp, stats.putsPerSecond, tags);
		QueueMetric(pps);

		name.assign("tsdb.internal.writespersecond");
		Metric wps(name, timestamp, stats.writesPerSecond, tags);
		QueueMetric(wps);

		name.assign("tsdb.internal.queuebacklog");
		Metric qbl(name, timestamp, stats.queueBacklog, tags);
		QueueMetric(qbl);
	}
}

void Datastore::Stop(void)
{
	spdlog::info("Datastore stopping");

	// finish writing all the data to disk
	Metric m[BULK_COUNT];
	std::size_t count = 0;
	while ((count = m_MetricQueue.try_dequeue_bulk(m, BULK_COUNT)) > 0)
	{
		for (std::size_t i = 0; i < count; i++)
		{
			// find the database in the cache
			datastore_t::iterator store = m_Store.find(m[i].Name());
			if (store != m_Store.end())
				WriteMetric(store->second, m[i]);
			else
			{
				// create the database
				dbconn *conn = CreateDatabase(m[i].Name());
				if (conn)
				{
					m_Store.insert(std::pair<std::string, dbconn*>(m[i].Name(), conn));
					WriteMetric(conn, m[i]);
				}
			}
		}
	}

	// close all database handles
	for (datastore_t::iterator ds = m_Store.begin();
		ds != m_Store.end(); ds++)
	{
		sqlite3_finalize(ds->second->insert);
		sqlite3_close_v2(ds->second->db);
		delete ds->second;
	}
	m_Store.clear();

	spdlog::info("Datastore stopped");
}
