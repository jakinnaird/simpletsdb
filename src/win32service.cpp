/*
 * Simple Time-Series Database
 *
 * Win32 Service
 *
 */

#if defined(_WIN32) || defined(WIN32)

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <sdkddkver.h>
#include <Shlobj.h>
#include <shlwapi.h>
#include <direct.h>
#include <tchar.h>
#include <Windows.h>
#include <winsock2.h>

#include "spdlog/spdlog.h"
#include "tclap/Cmdline.h"
#include "winreg/WinReg.hpp"

#include "eventlog.h"
#include "kernel.hpp"

#if defined(UNICODE) || defined(_UNICODE)
typedef std::wstring _tstring;
typedef std::wostringstream _toss;
#define _tcstombs	wcstombs
#define _mbstotcs	mbstowcs
#else
typedef std::string _tstring;
typedef std::ostringstream _toss;
#define _tcstombs	strncpy
#define _mbstotcs	strncpy
#endif

#define SERVICE_NAME			"SimpleTSDB"
#define SERVICE_DISPLAY_NAME	"Simple TSDB"
#define SERVICE_DESC			"Simple Time-Series Database"

SERVICE_STATUS_HANDLE g_hServiceStatus;
SERVICE_STATUS g_CurrentStatus;
HANDLE g_hStopEvent = NULL;
HANDLE g_hAppEventLog = NULL;

void WINAPI ServiceMain(DWORD argc, LPWSTR *argv);
void WINAPI ServiceControlHandler(DWORD control);
void ServiceInstall(bool install, const std::string &configFilePath);
void CheckCreateDirectory(const _tstring &path);

void WriteInfoEvent(uint32_t messageId, const _tstring &message);
void WriteErrorEvent(uint32_t messageId, const _tstring &message);
void WriteExceptionEvent(uint32_t messageId, const std::string &message);

char** CommandLineToArgvA(const char *cmdline, int *argc);

INT WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{
	// process the command line
	try
	{
		int argc = 0;
		char **argv = CommandLineToArgvA(GetCommandLineA(), &argc);

		TCLAP::CmdLine cmdline("SimpleTSDB", ' ', "none", false);
		TCLAP::ValueArg<std::string> installArg("", "install", "Install the SimpleTSDB service using the provided configuration file", false, ".", "Config file path");
		TCLAP::SwitchArg uninstallArg("", "uninstall", "Uninstall the SimpleTSDB service");
		TCLAP::ValueArg<std::string> configArg("", "config", "Path to configuration file", false, "", "unused");

		cmdline.add(configArg);
		cmdline.add(uninstallArg);
		cmdline.add(installArg);

		cmdline.parse(argc, argv);

		if (installArg.isSet())
		{
			ServiceInstall(true, installArg.getValue());
			return 0;
		}
		else if (uninstallArg.isSet())
		{
			ServiceInstall(false, "");
			return 0;
		}
	}
	catch (std::exception &e)
	{
		// unknown command line options, silently ignore
		e;
	}

	// start the service
	SERVICE_TABLE_ENTRY ste[] =
	{
		{ (LPTSTR)TEXT(SERVICE_NAME), ServiceMain },
		{ NULL, NULL, },
	};

	if (!StartServiceCtrlDispatcher(ste))
		return 1;

	return 0;
}

void WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	try
	{
		int argc = 0;
		char **argv = CommandLineToArgvA(GetCommandLineA(), &argc);

		// register as an event source
		g_hAppEventLog = RegisterEventSource(NULL, TEXT(SERVICE_NAME));

		WriteInfoEvent(MSG_STARTING, TEXT("SimpleTSDB service starting"));

		// determine the root working directory
		TCHAR szAppDataDir[MAX_PATH];
		TCHAR szAppLogDir[MAX_PATH];
		if (FAILED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA,
			NULL, 0, szAppDataDir)))
			throw std::runtime_error("Unable to determine data directory");

		// create the working directory, if needed
		PathAppend(szAppDataDir, TEXT("\\SimpleTSDB"));
		CheckCreateDirectory(szAppDataDir);

		// create the log working, if needed
		_tcscpy(szAppLogDir, szAppDataDir);
		PathAppend(szAppLogDir, TEXT("\\Log"));
		CheckCreateDirectory(szAppLogDir);

		// create the data working, if needed
		PathAppend(szAppDataDir, TEXT("\\Data"));
		CheckCreateDirectory(szAppDataDir);

		// General housekeeping
		g_hServiceStatus = RegisterServiceCtrlHandler(TEXT(SERVICE_NAME),
			ServiceControlHandler);
		if (g_hServiceStatus == NULL)
			throw std::runtime_error("Unable to register service status handle");

		ZeroMemory(&g_CurrentStatus, sizeof(SERVICE_STATUS));
		g_CurrentStatus.dwServiceSpecificExitCode = 0;
		g_CurrentStatus.dwWin32ExitCode = NO_ERROR;
		g_CurrentStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		g_CurrentStatus.dwWaitHint = 3000;
		g_CurrentStatus.dwCheckPoint = 0;

		g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (g_hStopEvent == NULL)
			throw std::runtime_error("Unable to create stop event");

		// Start up the service
		g_CurrentStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
		g_CurrentStatus.dwCurrentState = SERVICE_START_PENDING;
		SetServiceStatus(g_hServiceStatus, &g_CurrentStatus);

		// test to make sure that we can start the Winsock system
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
			throw std::runtime_error("Unable to start networking system");

		// determine the hostname
		char *_hostname = nullptr;
		DWORD bufsize = 0;
		if (!GetComputerNameExA(ComputerNamePhysicalDnsHostname,
			_hostname, &bufsize) && GetLastError() != ERROR_MORE_DATA)
			throw std::runtime_error("Unable to determine hostname");

		_hostname = new char[bufsize];
		if (_hostname == nullptr || !GetComputerNameExA(ComputerNamePhysicalDnsHostname,
			_hostname, &bufsize))
		{
			delete[] _hostname;
			throw std::runtime_error("Unable to determine hostname");
		}

		std::string hostname(_hostname);
		delete[] _hostname;

		char dataDir[MAX_PATH];
		char logDir[MAX_PATH];
		size_t ret = _tcstombs(dataDir, szAppDataDir, sizeof(dataDir));
		if (ret == MAX_PATH)
			dataDir[MAX_PATH - 1] = '\0';
		ret = _tcstombs(logDir, szAppLogDir, sizeof(logDir));
		if (ret == MAX_PATH)
			logDir[MAX_PATH - 1] = '\0';

		// convert all the command line args
		std::vector<std::string> args;
		for (int i = 0; i < argc; i++)
			args.push_back(argv[i]);

		Kernel kernel(args, logDir, dataDir, hostname);
		kernel.Start();

		g_CurrentStatus.dwCurrentState = SERVICE_RUNNING;
		SetServiceStatus(g_hServiceStatus, &g_CurrentStatus);

		WriteInfoEvent(MSG_RUNNING, TEXT("SimpleTSDB service running"));

		while (WaitForSingleObject(g_hStopEvent, 50) != WAIT_OBJECT_0)
		{
		}
	}
	catch (std::exception &e)
	{
		// log the error
		std::ostringstream oss;
		oss << "Exception: " << e.what();
		WriteExceptionEvent(MSG_EXCEPTION, oss.str());
	}
	catch (...)
	{
		// log the unhandled exception error
		WriteExceptionEvent(MSG_EXCEPTION, "Unhandled fatal exception");
	}

	WriteInfoEvent(MSG_STOPPING, TEXT("SimpleTSDB service stopping"));

	g_CurrentStatus.dwCurrentState = SERVICE_STOP_PENDING;
	SetServiceStatus(g_hServiceStatus, &g_CurrentStatus);

	WSACleanup();

	CloseHandle(g_hStopEvent);

	g_CurrentStatus.dwControlsAccepted = 0;
	g_CurrentStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(g_hServiceStatus, &g_CurrentStatus);

	WriteInfoEvent(MSG_STOPPED, TEXT("SimpleTSDB service stopped"));

	DeregisterEventSource(g_hAppEventLog);
}

void WINAPI ServiceControlHandler(DWORD control)
{
	switch (control)
	{
	case SERVICE_CONTROL_STOP:
		SetEvent(g_hStopEvent);
		break;
	}
}

void ServiceInstall(bool install, const std::string &configFilePath)
{
	SC_HANDLE schManager;
	SC_HANDLE schService;

	schManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schManager == NULL)
		return;

	if (install)
	{
		TCHAR szPath[MAX_PATH];
		if (!GetModuleFileName(NULL, szPath, MAX_PATH))
			return;

		TCHAR szConfigPath[MAX_PATH];
		size_t ret = _mbstotcs(szConfigPath, configFilePath.c_str(), sizeof(szConfigPath) / sizeof(TCHAR));
		if (ret == MAX_PATH)
			szConfigPath[MAX_PATH - 1] = TEXT('\0');

		// build the binary path
		_toss path;
		path << TEXT("\"") << szPath << TEXT("\"") << TEXT(" --config ")
			<< TEXT("\"") << szConfigPath << TEXT("\"");

		schService = CreateService(schManager, TEXT(SERVICE_NAME),
			TEXT(SERVICE_DISPLAY_NAME), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
			SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path.str().c_str(), NULL,
			NULL, NULL, NULL, NULL);
		if (schService)
		{
			SERVICE_DESCRIPTION sd;
			sd.lpDescription = (LPTSTR)TEXT(SERVICE_DESC);
			ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &sd);

			CloseServiceHandle(schService);
		}

		// create the required event log registry keys
		winreg::RegKey root{ HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application" };
		root.Create(root.Get(), TEXT(SERVICE_NAME));

		root.SetExpandStringValue(L"EventMessageFile", szPath);
		root.SetDwordValue(L"TypesSupported", 7);
	}
	else
	{
		schService = OpenService(schManager, TEXT(SERVICE_NAME), DELETE);
		if (schService)
		{
			DeleteService(schService);
			CloseServiceHandle(schService);
		}

		// delete the event log registry key
		winreg::RegKey root{ HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application" };
		root.DeleteKey(TEXT(SERVICE_NAME), KEY_ALL_ACCESS);
	}

	CloseServiceHandle(schManager);
}

void CheckCreateDirectory(const _tstring &path)
{
	DWORD attrib = GetFileAttributes(path.c_str());
	if (!(attrib != INVALID_FILE_ATTRIBUTES &&
		attrib & FILE_ATTRIBUTE_DIRECTORY))
	{
		// path doesn't exist, so create it
		if (!CreateDirectory(path.c_str(), NULL))
		{
			char _path[MAX_PATH];
			size_t ret = wcstombs(_path, path.c_str(), sizeof(_path));
			if (ret == MAX_PATH)
				_path[MAX_PATH-1] = '\0';

			std::ostringstream oss;
			oss << "Unable to create directory: '" << _path << "'";
			throw std::runtime_error(oss.str());
		}
	}
}

void WriteInfoEvent(uint32_t messageId, const _tstring &message)
{
	if (g_hAppEventLog)
	{
		LPCTSTR lpszStrings[2];
		WORD count = 1;

		lpszStrings[0] = TEXT(SERVICE_NAME);

		if (!message.empty())
		{
			lpszStrings[1] = message.c_str();
			count++;
		}

		ReportEvent(g_hAppEventLog,
			EVENTLOG_INFORMATION_TYPE,
			0,
			messageId,
			NULL,
			count,
			0,
			lpszStrings,
			NULL);
	}
}

void WriteErrorEvent(uint32_t messageId, const _tstring &message)
{
	if (g_hAppEventLog)
	{
		LPCTSTR lpszStrings[2];
		WORD count = 1;

		lpszStrings[0] = TEXT(SERVICE_NAME);

		if (!message.empty())
		{
			lpszStrings[1] = message.c_str();
			count++;
		}

		ReportEvent(g_hAppEventLog,
			EVENTLOG_ERROR_TYPE,
			0,
			messageId,
			NULL,
			count,
			0,
			lpszStrings,
			NULL);
	}
}

void WriteExceptionEvent(uint32_t messageId, const std::string &message)
{
	if (g_hAppEventLog)
	{
		LPCSTR lpszStrings[2];
		WORD count = 1;

		lpszStrings[0] = SERVICE_NAME;

		if (!message.empty())
		{
			lpszStrings[1] = message.c_str();
			count++;
		}

		ReportEventA(g_hAppEventLog,
			EVENTLOG_ERROR_TYPE,
			0,
			messageId,
			NULL,
			count,
			0,
			lpszStrings,
			NULL);
	}
}

char** CommandLineToArgvA(const char *cmdline, int *argc)
{
	// based on http://alter.org.ua/docs/win/args/
	char **argv;
	char *_argv;
	size_t len;
	int count;
	char a;
	size_t i, j;

	bool quotes;
	bool text;
	bool space;

	if (cmdline == NULL)
	{
		// we always return something
		char binary[65534];
		DWORD len = GetModuleFileNameA(NULL, binary,
			sizeof(binary) / sizeof(char));
		if (len)
			cmdline = binary;
	}

	len = strlen(cmdline);
	i = ((len + 2) / 2) * sizeof(void*) + sizeof(void*);

	argv = (char**)GlobalAlloc(GMEM_FIXED,
		i + (len + 2) * sizeof(char));

	_argv = (char*)(((unsigned char*)argv) + i);

	count = 0;
	argv[count] = _argv;
	quotes = false;
	text = false;
	space = true;
	i = 0;
	j = 0;

	while (a = cmdline[i])
	{
		if (quotes)
		{
			if (a == '\"')
				quotes = false;
			else
			{
				_argv[j] = a;
				j++;
			}
		}
		else
		{
			switch (a)
			{
			case '\"':
				quotes = true;
				text = true;
				if (space)
				{
					argv[count] = _argv + j;
					count++;
				}
				space = false;
				break;
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				if (text)
				{
					_argv[j] = '\0';
					j++;
				}
				text = false;
				space = true;
				break;
			default:
				text = true;
				if (space)
				{
					argv[count] = _argv + j;
					count++;
				}
				_argv[j] = a;
				j++;
				space = false;
				break;
			}
		}

		i++;
	}

	_argv[j] = '\0';
	argv[count] = NULL;

	if (argc)
		*argc = count;

	return argv;
}

#endif
