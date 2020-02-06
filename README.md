# simpletsdb
A simple and efficient time-series database daemon.

Loosely follows the API of OpenTSDB - telnet and HTTP put is supported. 

## Currently in early alpha
Builds under Visual Studio 2017 on Win64. Only supports writing data, querying is not implemented.

### Building on Windows
Open msw/simpletsdb.sln and build the configuration you want. Output ends up in package/bin/msw/<configuration>

### Installation on Windows
SimpleTSDB runs as a service. To install, open a shell prompt and run

> simpletsdb.exe --install `<path to config file>`

If the config file does not exist, the service starts with reasonable defaults:

- Default configuration file is searched for at `C:\ProgramData\SimpleTSDB\Data\stsdb.conf`
- Binds to localhost (127.0.0.1)
- Telnet port is 2181
- HTTP port is 8080
- Log directory is `C:\ProgramData\SimpleTSDB\Log`
- Data directory is `C:\ProgramData\SimpleTSDB\Data`

Start the service using

> net start simpletsdb

### Uninstallation on Windows
> net stop simpletsdb

> simpletsdb.exe --uninstall

The data and log files are untouched.

# Performance
Under Windows 10 Pro with an i5 processor, 8GB of RAM, and an SSD drive, metric write throughput can handle 1500+ writes/second, while the put throughput easily exceeds 2000+ metrics/second.

Statistics can be retrieved from the HTTP interface at 

> http://<bind_address>:<http_port>/api/stats

The HTTP response refreshes every 5 seconds.

Testing is done using https://github.com/staticmukesh/opentsdb-load-generator

