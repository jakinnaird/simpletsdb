/*
 * Simple Time-Series Database
 *
 * Thread
 *
 */

#include "Thread.hpp"

#if defined(_WIN32) || defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define SLEEP(x)	::Sleep(x)
#define THREAD_PROC	DWORD WINAPI ThreadEntry(LPVOID param)
#define THREAD_RET	return 0;
#else
#include <pthread.h>
#include <unistd.h>
#define SLEEP(x)	::usleep(1000 * x)
#define THREAD_PROC void* ThreadEntry(void *param)
#define THREAD_RET	return NULL;
#endif

void ThreadProc::Sleep(uint32_t msec)
{
	SLEEP(msec);
}

class ThreadHelper
{
private:
	Thread *m_Thread;

public:
	ThreadHelper(Thread *thread)
		: m_Thread(thread) {}

	void Exec(void)
	{
		m_Thread->OnStart();
		m_Thread->OnProcess();
		m_Thread->OnStop();
	}
};

THREAD_PROC
{
	ThreadHelper *helper = (ThreadHelper*)param;
	if (helper)
		helper->Exec();

	THREAD_RET
}

Thread::Thread(ThreadProc *proc)
{
	m_Proc = proc;
	m_Helper = nullptr;
	m_Handle = NULL;

	m_Started = false;
	m_Running = false;
	m_Pausing = false;
	m_Paused = false;
}

Thread::~Thread(void)
{
	Stop();
}

bool Thread::Start(void)
{
	if (m_Proc == nullptr)
		return false;

	if (m_Started)
		return true;

	m_Helper = new ThreadHelper(this);
	if (m_Helper == nullptr)
		return false;

	m_Started = true;

#if defined(_WIN32) || defined(WIN32)
	m_Handle = CreateThread(NULL, 0, ThreadEntry, m_Helper,
		0, NULL);
#else
	m_Handle = MemoryManager::Alloc(sizeof(pthread_t));
	if (pthread_create((pthread_t*)m_Handle, NULL, ThreadEntry,
		m_Helper))
		return false;
#endif

	if (m_Handle == NULL)
	{
		m_Started = false;
		return false;
	}

	m_Running = true;
	return true;
}

void Thread::Stop(void)
{
	m_Running = false;
	m_Started = false;

	if (m_Handle)
	{
#if defined(_WIN32) || defined(WIN32)
		WaitForSingleObject((HANDLE)m_Handle, INFINITE);
		CloseHandle((HANDLE)m_Handle);
#else
		pthread_join(*((pthread_t*)m_Handle), NULL);
		MemoryManager::Free(m_Handle);
#endif
				
		m_Handle = nullptr;
	}

	if (m_Helper)
	{
		delete m_Helper;
		m_Helper = nullptr;
	}
}

void Thread::Pause(void)
{
	m_Pausing = true;
	int8_t watchdog = 0;

	while (!m_Paused)
	{
		if (watchdog++ < 5)	// wait for 5 seconds
			SLEEP(1000);
		else
			break;
	}
}

void Thread::Resume(void)
{
	m_Pausing = false;
	m_Paused = false;
}

void Thread::OnStart(void)
{
	if (m_Started)
		m_Proc->Start();
}

void Thread::OnProcess(void)
{
	while (m_Started)
	{
		if (m_Running && !m_Paused)
			m_Proc->Process();
		else
			SLEEP(50);

		if (m_Pausing)
			m_Paused = true;
		
		SLEEP(0);	// release our time slice
	}
}

void Thread::OnStop(void)
{
	m_Proc->Stop();
}
