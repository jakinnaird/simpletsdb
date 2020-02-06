/*
 * Simple Time-Series Database
 *
 * Datastore
 *
 */

#pragma once

#include <atomic>
#include <map>
#include <string>

#include "metric.hpp"
#include "stats.hpp"
#include "thread.hpp"

#include "concurrentqueue.h"
#include "sqlite3.h"

class Datastore : public ThreadProc
{
private:
	struct dbconn
	{
		sqlite3 *db;
		sqlite3_stmt *insert;
	};

private:
	std::string m_DataDir;
	std::string m_DbExt;
	std::string m_Hostname;

	moodycamel::ConcurrentQueue<Metric> m_MetricQueue;
	std::atomic_size_t m_QueueSize;

	Statistics *m_Stats;

	typedef std::map<std::string, dbconn*> datastore_t;
	datastore_t m_Store;

	bool m_Running;
	Thread *m_Thread;

public:
	Datastore(const std::string &dataDir, const std::string &dbExt,
		const std::string &hostname, Statistics *stats);
	~Datastore(void);

	bool StartThread(void);
	void StopThread(void);

	void QueueMetric(const Metric &metric);

private:
	bool CacheDatabase(const std::string &name, const std::string &path);
	dbconn* CreateDatabase(const std::string &name);

	void WriteMetric(dbconn *conn, const Metric &metric);

protected:
	void Start(void);
	void Process(void);
	void Stop(void);
};
