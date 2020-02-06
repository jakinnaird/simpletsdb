/*
 * Simple Time-Series Database
 *
 * Statistics
 *
 */

#pragma once

#include <atomic>
#include <cstdint>

#include "thread.hpp"
#include "timer.hpp"

class Statistics : public ThreadProc
{
public:
	struct Stats
	{
		double putsPerSecond;
		double writesPerSecond;

		double queueBacklog;
	};

private:
	Timer m_Timer;
	float m_LastTime;
	std::atomic_bool m_Updated;

	Stats m_Stats;

	std::atomic_size_t m_PutCount;
	std::atomic_size_t m_WriteCount;
	std::atomic_size_t m_QueueBacklog;

	Thread *m_Thread;

public:
	Statistics(void);
	~Statistics(void);

	bool StartThread(void);
	void StopThread(void);

	bool GetStats(Statistics::Stats &stats,
		bool updated = false);

	void AddPutCount(std::size_t count);
	void AddWriteCount(std::size_t count);

	void SetQueueBacklog(std::size_t count);

protected:
	void Start(void);
	void Process(void);
	void Stop(void);
};
