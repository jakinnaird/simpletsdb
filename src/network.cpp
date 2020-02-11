/*
 * Simple Time-Series Database
 *
 * Network processor
 *
 */

#include "downsampler.hpp"
#include "metric.hpp"
#include "network.hpp"
#include "query.hpp"
#include "thread.hpp"
#include "utility.hpp"

#include "civetweb.h"
#include "json/json.hpp"
#include "spdlog/spdlog.h"

#include <map>
#include <sstream>

#if defined(_WIN32) || defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define CLOSE_SOCKET	closesocket
typedef SOCKET socket_t;
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define CLOSE_SOCKET	close
#define int32_t socket_t;
#endif

bool isValidChar(char c)
{
	unsigned char ch = (unsigned char)c;
	if ((isalnum(ch) || (ispunct(ch) && ch != '\'') || ch == ' ') && (ch != '\n' || ch != '\r'))
		return false;

	return true;
}

class TelnetProcessor : public ThreadProc
{
private:
	std::string m_BindAddr;
	std::string m_BindPort;

	Datastore *m_DataStore;
	Statistics *m_Stats;

	bool m_Running;

	socket_t m_Listener;
	fd_set m_Master;
	int32_t m_Socket_max;
	int32_t m_Backlog;

	typedef std::map<socket_t, std::string> clientbuffers_t;
	clientbuffers_t m_ClientBuffers;

	Thread *m_Thread;

public:
	TelnetProcessor(const std::string &bindAddr, const std::string &port,
		int32_t backlog, Datastore *datastore, Statistics *stats)
		: m_BindAddr(bindAddr), m_BindPort(port), m_Backlog(backlog),
		  m_DataStore(datastore), m_Stats(stats)
	{
		m_Listener = 0;
		FD_ZERO(&m_Master);
		m_Socket_max = 0;

		m_Running = false;

		m_Thread = new Thread(this);
		if (m_Thread == nullptr)
			throw std::runtime_error("Failed to create telnet processor thread");
	}

	~TelnetProcessor(void)
	{
		delete m_Thread;
	}

	bool StartThread(void)
	{
		return m_Thread->Start();
	}

	void StopThread(void)
	{
		spdlog::info("Telnet interface stopping");

		m_Thread->Stop();

		spdlog::info("Telnet interface stopped");
	}

	void Start(void)
	{
		spdlog::info("Starting telnet interface on {0}:{1}",
			m_BindAddr.c_str(), m_BindPort.c_str());

#if defined(_WIN32) || defined(WIN32)
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 0), &wsa) != 0)
		{
			spdlog::error("Failed to initialize socket library: {0}", WSAGetLastError());
			return;
		}
#endif

		int32_t yes = 1;

		struct addrinfo hints, *ai, *p;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;

		if (getaddrinfo(m_BindAddr.c_str(), m_BindPort.c_str(), &hints, &ai) != 0)
		{
			spdlog::error("Failed to getaddrinfo: {0}", WSAGetLastError());
			return;
		}

		for (p = ai; p != nullptr; p = p->ai_next)
		{
			m_Listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (m_Listener < 0)
				continue;

			setsockopt(m_Listener, SOL_SOCKET, SO_REUSEADDR, 
				(const char*)&yes, sizeof(int32_t));

			if (bind(m_Listener, p->ai_addr, p->ai_addrlen) < 0)
			{
				CLOSE_SOCKET(m_Listener);
				continue;
			}

			break;
		}

		if (p == nullptr)
		{
			spdlog::error("Failed to bind telnet socket: {0}", WSAGetLastError());
			return;
		}

		freeaddrinfo(ai);

		FD_SET(m_Listener, &m_Master);
		m_Socket_max = m_Listener;

		if (listen(m_Listener, m_Backlog) == -1)
		{
			spdlog::error("Failed to start listening: {0}", WSAGetLastError());
			return;
		}

		spdlog::info("Telnet interface running");

		m_Running = true;
	}

	void Process(void)
	{
		if (!m_Running)
		{
			m_Thread->Stop();
			return;
		}

		// process the telnet interface
		struct timeval timeout = { 0, 50000 };
		fd_set readfds = m_Master;
		if (select(m_Socket_max + 1, &readfds, nullptr, nullptr, &timeout) == -1)
		{
			spdlog::warn("Telnet select() failed");
			return;
		}
	
		for (socket_t sock = 0; sock <= m_Socket_max; sock++)
		{
			if (FD_ISSET(sock, &readfds))
			{
				if (sock == m_Listener)
				{
					// new connection
					struct sockaddr_storage remoteaddr;
					socklen_t addrlen = sizeof(remoteaddr);
					socket_t newfd = accept(m_Listener,
						(struct sockaddr*)&remoteaddr, &addrlen);
					if (newfd == -1)
						spdlog::warn("Failed to accept new telnet connection");
					else
					{
						FD_SET(newfd, &m_Master);
						if (newfd > m_Socket_max)
							m_Socket_max = newfd;
	
						m_ClientBuffers.insert(std::pair<socket_t, std::string>(
							newfd, std::string()));
	
						char remoteIP[INET6_ADDRSTRLEN];
						spdlog::debug("new connection from {0} on socket {1}",
							inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),
								remoteIP, sizeof(remoteIP)), newfd);
					}
				}
				else
				{
					// existing connection has new data
					char buf[2048];
					memset(buf, 0, sizeof(buf));
					int32_t nbytes = recv(sock, buf, sizeof(buf), 0);
					if (nbytes <= 0)
					{
						if (nbytes == 0)
							spdlog::debug("socket {0} hung up", sock);
						else
							spdlog::warn("Failed to read on socket {0}", sock);
	
						CLOSE_SOCKET(sock);
						FD_CLR(sock, &m_Master);
						m_ClientBuffers.erase(sock);
					}
					else
					{
						std::string &line = m_ClientBuffers[sock];
						for (int32_t c = 0; c < nbytes; c++)
						{
							// cast the buffer value to unsigned char to prevent assertions
							unsigned char ch = (unsigned char)buf[c];
							if ((isalnum(ch) || (ispunct(ch) && ch != '\'') || ch == ' ') && (ch != '\n' || ch != '\r'))
							{
								line.append(1, ch);
							}
							else if (ch == '\n')
							{
								// sometimes the first character is non-alpha, remove it
								if (!isalpha(line[0]) || ispunct(line[0]) || line[0] == '\'')
									line = line.substr(1);
	
								if (line.find("put") == 0)
								{
									// ensure there are at least 5 fields
									// put <metric> <timestamp> <value> <tagk_1=tagv_1> [<tagk_n=tagv_n>]
									int32_t param_count = 1;
									std::string::size_type pos = line.find(' ');
									while (pos != std::string::npos)
									{
										param_count++;
										pos = line.find(' ', pos + 1);
									}
	
									if (param_count < 5)
									{
										std::ostringstream err;
										err << "put: invalid number of parameters (" << param_count << "), 5 required.\n\r";
										send(sock, err.str().c_str(), err.str().length() + 1, 0);
									}
									else
									{
										std::string error;
										line = line.substr(4);
										Metric metric(line);
										if (metric.IsValid(error))
										{
											// queue this metric
											m_DataStore->QueueMetric(metric);

											m_Stats->AddPutCount(1);
										}
										else
										{
											std::ostringstream err;
											err << "put: invalid value: " << error << "\r\n";
											send(sock, err.str().c_str(), err.str().length() + 1, 0);
										}
									}
								}
								else
								{
									// @TODO
									// check to see if there are any requests we can handle
									// e.g. status, stats, etc.
								}
	
								line.clear();
							}
						}
					}
				}
			}
		}
	}

	void Stop(void)
	{
		// close all client sockets
		for (clientbuffers_t::iterator client = m_ClientBuffers.begin();
			client != m_ClientBuffers.end(); ++client)
		{
			CLOSE_SOCKET(client->first);
		}
		m_ClientBuffers.clear();

		if (m_Listener)
		{
			CLOSE_SOCKET(m_Listener);
			m_Listener = 0;
		}

#if defined(_WIN32) || defined(WIN32)
		WSACleanup();
#endif
	}

protected:
	void *get_in_addr(struct sockaddr *sa)
	{
		if (sa->sa_family == AF_INET)
			return &(((struct sockaddr_in*)sa)->sin_addr);

		return &(((struct sockaddr_in6*)sa)->sin6_addr);
	}
};

class HttpProcessor
{
private:
	Datastore *m_DataStore;
	Statistics *m_Stats;

	std::string m_BindAddr;
	std::string m_BindPort;

	mg_context *m_Ctx;
	mg_callbacks m_Callbacks;

public:
	HttpProcessor(const std::string &bindAddr, const std::string &port,
		Datastore *datastore, Statistics *stats)
		: m_BindAddr(bindAddr), m_BindPort(port), m_DataStore(datastore),
		m_Stats(stats)
	{
		m_Ctx = nullptr;
	}

	~HttpProcessor(void)
	{
	}

	bool StartThread(void)
	{
		std::string bind(m_BindAddr);
		bind.append(":");
		bind.append(m_BindPort);

		spdlog::info("Starting HTTP interface on {0}", bind.c_str());

		const char *options[] =
		{
			"listening_ports", bind.c_str(),
			NULL
		};

		memset(&m_Callbacks, 0, sizeof(m_Callbacks));
		m_Callbacks.log_message = mg_log_message;
		m_Callbacks.log_access = mg_log_access;

		mg_init_library(0);

		m_Ctx = mg_start(&m_Callbacks, nullptr, options);
		if (m_Ctx == nullptr)
			return false;

		mg_set_request_handler(m_Ctx, "/api/aggregators", mg_aggregators_handler, this);
		mg_set_request_handler(m_Ctx, "/api/put", mg_put_handler, this);
		mg_set_request_handler(m_Ctx, "/api/query", mg_query_handler, this);
		mg_set_request_handler(m_Ctx, "/api/stats", mg_stats_handler, this);

		spdlog::info("HTTP interface running");
		return true;
	}

	void StopThread(void)
	{
		spdlog::info("HTTP interface stopping");

		if (m_Ctx)
		{
			mg_stop(m_Ctx);
			mg_exit_library();
			m_Ctx = nullptr;
		}

		spdlog::info("HTTP interface stopped");
	}

	static int mg_aggregators_handler(struct mg_connection *conn, void *cbdata)
	{
		HttpProcessor *http = static_cast<HttpProcessor*>(cbdata);
		if (http == nullptr)
			return mg_write_500(conn);

		return http->ApiAggregatorsHandler(conn);	// success
	}

	static int mg_put_handler(struct mg_connection *conn, void *cbdata)
	{
		HttpProcessor *http = static_cast<HttpProcessor*>(cbdata);
		if (http == nullptr)
			return mg_write_500(conn);

		return http->ApiPutHandler(conn);	// success
	}

	static int mg_query_handler(struct mg_connection *conn, void *cbdata)
	{
		HttpProcessor *http = static_cast<HttpProcessor*>(cbdata);
		if (http == nullptr)
			return mg_write_500(conn);

		return http->ApiQueryHandler(conn);	// success
	}

	static int mg_stats_handler(struct mg_connection *conn, void *cbdata)
	{
		HttpProcessor *http = static_cast<HttpProcessor*>(cbdata);
		if (http == nullptr)
			return mg_write_500(conn);

		return http->ApiStatsHandler(conn);	// success
	}

public:
	int32_t ApiAggregatorsHandler(struct mg_connection *conn)
	{
		const struct mg_request_info *request = mg_get_request_info(conn);
		if (strcmp(request->request_method, "GET") != 0 &&
			strcmp(request->request_method, "POST") != 0)
		{
			mg_printf(conn,
				"HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
			mg_printf(conn, "Error 405: %s requests not allowed for this endpoint.",
				request->request_method);

			return 405;	// this verb is not allowed
		}

		mg_printf(conn,
			"HTTP/1.1 200 OK\r\nContent-Type: text/text\r\nConnection: close\r\n\r\n");
		mg_printf(conn, "[\r\n");
		mg_printf(conn, "\t\"avg\",\r\n");
		mg_printf(conn, "\t\"min\",\r\n");
		mg_printf(conn, "\t\"max\",\r\n");
		mg_printf(conn, "\t\"sum\"\r\n");
		mg_printf(conn, "]\r\n");

		return 200;
	}

	int32_t ApiPutHandler(struct mg_connection *conn)
	{
		const struct mg_request_info *request = mg_get_request_info(conn);
		if (strcmp(request->request_method, "POST") != 0)
		{
			mg_printf(conn,
				"HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
			mg_printf(conn, "Error 405: %s requests not allowed for this endpoint.",
				request->request_method);
	
			return 405;	// this verb is not allowed
		}
	
		// read in the post data
		std::unique_ptr<char[]> buffer(new char[request->content_length + 1]);
		if (!buffer)
		{
			spdlog::warn("Failed to allocate buffer");
			return mg_write_500(conn);
		}
	
		memset(buffer.get(), 0, request->content_length + 1);
	
		int64_t len = mg_read(conn, buffer.get(), request->content_length);
		if (len > 0)
		{
			std::istringstream iss(buffer.get());
			std::string line;
			std::size_t count = 0;
			while (std::getline(iss, line))
			{
				// clean the line
				line.erase(std::remove_if(line.begin(), line.end(), isValidChar), line.end());
	
				// the line may be empty, so check that
				if (line.length() == 0)
					continue;

				std::string error;
				Metric metric(line);
				if (metric.IsValid(error))
				{
					++count;

					// queue this metric
					m_DataStore->QueueMetric(metric);

					// complete the request
					mg_printf(conn,
						"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
				}
				else
				{
					std::ostringstream err;
					err << "put: invalid value: " << error << "\r\n";
					return mg_write_400(conn, err.str());
				}
			}

			m_Stats->AddPutCount(count);
		}
	
		return 200;
	}
	
	int32_t ApiQueryHandler(struct mg_connection *conn)
	{
		const struct mg_request_info *request = mg_get_request_info(conn);
		if (strcmp(request->request_method, "GET") != 0)
		{
			mg_printf(conn,
				"HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
			mg_printf(conn, "Error 405: %s requests not allowed for this endpoint.",
				request->request_method);
	
			return 405;	// this verb is not allowed
		}
	
		// Step 1: break up the query string into parts
		std::string query(request->query_string);
		std::vector<std::string> parts;

		std::string::size_type start = 0;
		std::string::size_type pos = query.find('&', start);
		while (pos != std::string::npos)
		{
			std::string part = query.substr(start, pos - start);
			parts.push_back(part);

			start = pos + 1;
			pos = query.find('&', start);
		}

		if (start < query.length())
		{
			std::string part = query.substr(start);
			parts.push_back(part);
		}

		uint64_t curTime = time(nullptr);
		uint64_t startTime = 0;
		uint64_t endTime = curTime;	// by default, the end time is now
		std::vector<ResultSet*> subqueries;

		// Step 2: Identify the parts and process them
		for (std::vector<std::string>::iterator part = parts.begin();
			part != parts.end(); ++part)
		{
			if ((*part).find("start=") == 0)
			{
				startTime = curTime - ParseTime((*part).substr(6));
			}
			else if ((*part).find("end=") == 0)
			{
				endTime = curTime - ParseTime((*part).substr(4));
			}
			else if ((*part).find("m=") == 0)
			{
				Query q((*part).substr(2));

				ResultSet *rs = m_DataStore->PrepareQuery(q);
				if (rs)
					subqueries.push_back(rs);
			}
		}

		nlohmann::json response;

		// Step 4: Get the result sets
		for (std::vector<ResultSet*>::iterator resultset = subqueries.begin();
			resultset != subqueries.end(); ++resultset)
		{
			std::vector<ResultSet::dps> results;
			if ((*resultset)->Execute(startTime, endTime, results))
			{
				nlohmann::json jr;

				jr["metric"] = (*resultset)->GetMetric();

				// Step 5: downsample the data
				std::vector<ResultSet::dps> output;
				Downsampler ds((*resultset)->GetDownsampler());
				if (ds.Decimate(results, output) > 0)
				{
					// Step 6: serialize each result set
					for (std::vector<ResultSet::dps>::iterator dps = output.begin();
						dps != output.end(); ++dps)
					{
						std::ostringstream oss;
						oss << dps->timestamp;
						jr["dps"][oss.str()] = dps->value;
					}
				}

				response.push_back(jr);
			}

			delete (*resultset);
		}

		// Step 7: write the data to the client
		mg_printf(conn,
			"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
		mg_printf(conn, response.dump(1).c_str());

		return 200;
	}

	int32_t ApiStatsHandler(struct mg_connection *conn)
	{
		const struct mg_request_info *request = mg_get_request_info(conn);
		if (strcmp(request->request_method, "GET") != 0)
		{
			mg_printf(conn,
				"HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
			mg_printf(conn, "Error 405: %s requests not allowed for this endpoint.",
				request->request_method);

			return 405;	// this verb is not allowed
		}

		mg_printf(conn,
			"HTTP/1.1 200 OK\r\nContent-Type: text/text\r\nRefresh: 5;url=/api/stats\r\nConnection: close\r\n\r\n");

		Statistics::Stats stats;
		m_Stats->GetStats(stats);

		mg_printf(conn, "Puts/second: %.2f\r\n", stats.putsPerSecond);
		mg_printf(conn, "Writes/second: %.2f\r\n", stats.writesPerSecond);
		mg_printf(conn, "Queue backlog: %.2f\r\n", stats.queueBacklog);

		return 200;
	}

private:
	static int mg_write_500(struct mg_connection *conn)
	{
		mg_printf(conn,
			"HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
		mg_printf(conn, "Error 500: Internal Server Error.");

		return 500;	// this shouldn't have happened
	}

	static int mg_write_400(struct mg_connection *conn, const std::string &error)
	{
		mg_printf(conn,
			"HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
		mg_printf(conn, "Error 400: %s", error.c_str());

		return 400;	// this shouldn't have happened
	}

	static int mg_log_message(const struct mg_connection *conn, const char *message)
	{
		spdlog::info(message);
		return 0;
	}

	static int mg_log_access(const struct mg_connection *conn, const char *message)
	{
		spdlog::debug(message);
		return 0;
	}
};

NetworkProcessor::NetworkProcessor(const std::string &bindAddr,
	Datastore *datastore, Statistics *stats)
	: m_BindAddr(bindAddr), m_DataStore(datastore), m_Stats(stats)
{
	m_Telnet = nullptr;
	m_Http = nullptr;
}

NetworkProcessor::~NetworkProcessor(void)
{
}

bool NetworkProcessor::StartTelnetInterface(const std::string &port,
	uint32_t backlog)
{
	if (port == "0")
		return true; // we are not starting this up

	m_Telnet = new TelnetProcessor(m_BindAddr, port, backlog,
		m_DataStore, m_Stats);
	if (m_Telnet == nullptr)
		return false;

	return m_Telnet->StartThread();
}

bool NetworkProcessor::StartHTTPInterface(const std::string &port)
{
	if (port == "0")
		return true; // we are not starting this up

	m_Http = new HttpProcessor(m_BindAddr, port, m_DataStore, m_Stats);
	if (m_Http == nullptr)
		return false;

	return m_Http->StartThread();
}

void NetworkProcessor::StopTelnetInterface(void)
{
	if (m_Telnet)
	{
		m_Telnet->StopThread();
		delete m_Telnet;
		m_Telnet = nullptr;
	}
}

void NetworkProcessor::StopHTTPInterface(void)
{
	if (m_Http)
	{
		m_Http->StopThread();
		delete m_Http;
		m_Http = nullptr;
	}
}
