/*
 * Simple Time-Series Database
 *
 * Thread
 *
 */

#pragma once

#include <cstdint>

class ThreadProc
{
public:
	virtual ~ThreadProc(void) {}

	virtual void Start(void) = 0;
	virtual void Process(void) = 0;
	virtual void Stop(void) = 0;

	void Sleep(uint32_t msec);
};

class ThreadHelper;

class Thread
{
	friend class ThreadHelper;

private:
	ThreadProc *m_Proc;
	ThreadHelper *m_Helper;
	void *m_Handle;

	bool m_Started;
	bool m_Running;
	bool m_Pausing;
	bool m_Paused;

public:
	Thread(ThreadProc *proc);
	~Thread(void);

	bool Start(void);
	void Stop(void);

	void Pause(void);
	void Resume(void);

protected:
	void OnStart(void);
	void OnProcess(void);
	void OnStop(void);
};
