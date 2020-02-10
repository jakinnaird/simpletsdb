# SimpleTSDB
A simple and efficient time-series database daemon supporting 1 second resolution.

Loosely follows the API of OpenTSDB - telnet and HTTP put is supported. HTTP queries support a reduced subset of options.

## Currently in early alpha
Builds under Visual Studio 2017 on Win64.

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

# Writing data
## Telnet interface

Writing data using the telnet interface is simple. A single line is a single data point.

The format is as follows:
> put `<metric name>` `<timestamp>` `<value>` `<[tagk=value,...>`

List of parameters:

| Parameter | Description |
| --------- | ----------- |
| metric name | The name of the metric to write. Must not contain whitespace. |
| timestamp | The timestamp for the data point. Must be in Unix epoch format. 1 second resolution. |
| value | The value for this data point. Must be a number, and is stored as a floating point. |
| tagk=value | A set of key/value pairs. At least one key/value pair is required. Additional pairs are separated by a space. |

Example:
> put sys.cpu.nice 11583988 23.4 host=web01

An accepted datapoint does not return any output. An error is written to back to the client when the call is incorrect.

## HTTP interface

Using the HTTP interface, multiple data points can be written in one call. Each data point can be for a different metric.

Data is written using a POST request to the `/api/put` endpoint. Each data point is a single line in the post data, with each line following the telnet put format, without the leading 'put'.

Example post data being written to `http://localhost:8080/api/put`

```
sys.cpu.nice 11583988 23.4 host=web01 dc=ny
sys.cpu.idle 11583988 12.9 host=web01 dc=ny
sys.cpu.nice 11583988 37.1 host=web02 dc=lon
```

# Querying results

Querying is done using the HTTP interface endpoint `/api/query`.

## Query requests
Currently, only GET requests are supported. The query string format is as follows:

> start=`<timestamp>`&end=`<timestamp>`&m=`<aggregator>`:`<metric name>{tag filter}`:[downsampler]

| Parameter | Description | Required | Default |
| --------- | ----------- | -------- | ------- |
| start | The start time for the result set. Can be either in Unix epoch format, or relative format. See below for relative format description. | True | |
| end | The end time for the result set. Same rules as start time. | False | 0 (all data up to now) |
| aggregator | The aggregation method to be used. See below for details. | True | |
| metric name | The name of the metric to return. | True | |
| tag filter | A comma separated list of key/value pairs to filter the returned data (logical AND). See below for details. | True | |
| downsampler | This optional field downsamples the data, reducing the number of data points returned. See below for details. | False | |

Examples:
> http://localhost:8080/api/query?start=1151234&m=sum:sys.cpu.nice{host=web*,dc=ny}

> http://localhost:8080/api/query?start=1h-ago&m=sum:sys.cpu.nice{host=web*,dc=ny|lon}:5m-avg

Multiple metric requests can be made in the same call by including additional `m=` parameters.

Example:
> http://localhost:8080/api/query?start=1151234&m=sum:sys.cpu.nice{host=web*,dc=ny}&m=sum:sys.cpu.nice{dc=ny|dc=lon}:5m-avg

### Start and end time format
The start and end times can be either absolute Unix epoch timestamps (1 second resolution), or in a relative format (\<size\>\<unit\>-ago).

Relative example:
> 1h-ago

Currently, the following relative markers are supported:

| Marker | Description | Example |
| ------ | ----------- | ------- |
| s | Seconds | 30s-ago |
| m | Minutes | 15m-ago |
| h | Hours | 12h-ago |
| d | days | 7d-ago |

### Aggregation
Metrics are aggregated using the following options:

| Method | Description |
| ------ | ----------- |
| avg | The average of all datapoints for that timestamp. |
| min | The minimum value of all datapoints for that timestamp. |
| max | The maximum value of all datapoints for that timestamp. |
| sum | The sum of all datapoints for that timestamp. |

The currently supported list can be found by querying the HTTP endpoint `/api/aggregators`

### Tag filters
| Example | Description |
| ------- | ----------- |
| \<tagk\>=* | Wildcard filter. Effectively enforces that the tag exists in the series. |
| \<tagk\>=value | Case insensitive literal OR filter |
| \<tagk\>=value1\|value2\|value3 | Case insensitive literal OR filter |
| \<tagk\>=va* | Case insensitive wildcard filter. |

### Downsampling

`Currently not implemented`


Metrics can be downsampled to different time intervals. For example, data with 1 second resolution can be returned with aggregation over a 5 minute period.

Downsamplers require 2 components:
 * Interval - A time range for which the downsampling will aggregate values. Intervals are specified in the form \<size\>\<units\>, such as `5m` for 5 minutes, or `1h` for 1 hour. There is an additional interval `all` that applies the aggregation function to all data points in the query. For the `all` interval, a size must be provided, but is ignored (e.g. `0all`).
 * Aggregator - The aggregation method to be applied for each interval (see above for available methods).

Examples:
> 1h-avg

> 30m-sum

> 0all-sum

## Query response

`Currently, the result set is written as plain-text with one timestamp/value pair per line`

# Performance
Under Windows 10 Pro with an i5 processor, 8GB of RAM, and an SSD drive, metric write throughput can handle 1500+ writes/second, while the put throughput easily exceeds 2000+ metrics/second.

Statistics can be retrieved from the HTTP interface at 

> http://<bind_address>:<http_port>/api/stats

The HTTP response refreshes every 5 seconds.

Testing is done using https://github.com/staticmukesh/opentsdb-load-generator
