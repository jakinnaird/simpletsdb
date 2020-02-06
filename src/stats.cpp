/*
 * Simple Time-Series Database
 *
 * Statistics
 *
 */

#include <stdexcept>

#include "stats.hpp"

Statistics::Statistics(void)
{
	m_LastTime = m_Timer.Elapsed();
	memset(&m_Stats, 0, sizeof(m_Stats));

	m_PutCount = 0;
	m_WriteCount = 0;
	m_QueueBacklog = 0;

	m_Thread = new Thread(this);
	if (m_Thread == nullptr)
		throw std::runtime_error("Failed to create statistics thread");
}

Statistics::~Statistics(void)
{
	delete m_Thread;
}

bool Statistics::StartThread(void)
{
	return m_Thread->Start();
}

void Statistics::StopThread(void)
{
	m_Thread->Stop();
}

bool Statistics::GetStats(Stats &stats, bool updated)
{
	if (updated && !m_Updated)
		return false;

	memcpy(&stats, &m_Stats, sizeof(Stats));
	m_Updated = false;
	return true;
}

void Statistics::AddPutCount(std::size_t count)
{
	m_PutCount += count;
}

void Statistics::AddWriteCount(std::size_t count)
{
	m_WriteCount += count;
}

void Statistics::SetQueueBacklog(std::size_t count)
{
	m_QueueBacklog = count;
}

void Statistics::Start(void)
{
	// nothing to do
}

void Statistics::Process(void)
{
	float curTime = m_Timer.Elapsed();
	float deltaTime = curTime - m_LastTime;
	if (deltaTime >= 1.0f)
	{
		// one second has passed, so update stats
		m_Stats.putsPerSecond = m_PutCount / deltaTime;
		m_Stats.writesPerSecond = m_WriteCount / deltaTime;
		m_Stats.queueBacklog = (double)m_QueueBacklog;

		// reset the values
		m_PutCount = 0;
		m_WriteCount = 0;

		m_LastTime = curTime;
		m_Updated = true;
	}
	else
		Sleep(50);
}

void Statistics::Stop(void)
{
	// nothing to do
}
