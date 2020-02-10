/*
 * Simple Time-Series Database
 *
 * Query
 *
 */

#include "query.hpp"
#include "utility.hpp"

#include "spdlog/spdlog.h"
#include "sql-builder/sql.h"

#include <sstream>

Query::Query(const std::string &query)
{
	sql::SelectModel sql;
	sql.from("METRIC");
	sql.group_by("timestamp");
	sql.where("(timestamp >= ?001 and timestamp <= ?002)");

	// process the query string
	// it's a fixed format, so we can make assumptions
	std::vector<std::string> elems;
	std::size_t count = SplitString(query, ':', elems);
	if (count < 2)
	{
		spdlog::info("Invalid query string: {0}", query.c_str());
		return;
	}
	else
	{
		std::string agg(elems[0]);
		agg.append("(value) as value");
		sql.select("timestamp", agg);

		// get the metric name
		std::string::size_type ob = elems[1].find('{');
		std::string::size_type cb = elems[1].find('}', ob);
		if (ob == std::string::npos || cb == std::string::npos)
		{
			spdlog::info("A filter is required. {0}", query.c_str());
			return;
		}

		m_Metric = elems[1].substr(0, ob);

		// get the tags
		std::string tags = elems[1].substr(ob + 1, cb - ob - 1);

		std::vector<std::string> filters;
		std::size_t count = SplitString(tags, ',', filters);
		if (count < 1)
		{
			spdlog::info("Invalid number of filters: {0}", tags.c_str());
			return;
		}
		else
		{
			for (std::vector<std::string>::iterator filter = filters.begin();
				filter != filters.end(); ++filter)
			{
				std::ostringstream oss;

				// replace the wildcard with something we use internally
				std::replace((*filter).begin(), (*filter).end(), '*', '%');

				// split the filter into parts
				std::vector<std::string> filterparts;
				std::size_t fp = SplitString((*filter), '=', filterparts);
				if (fp < 2 || fp > 2)
				{
					spdlog::info("Invalid key/value pair: {0}", (*filter).c_str());
					continue;
				}
				else
				{
					// check to see if there is a | in the filter and update accordingly
					std::vector<std::string> orparts;
					std::size_t op = SplitString(filterparts[1], '|', orparts);
					if (op > 0)
					{
						oss << "and (";

						for (std::size_t i = 0; i < orparts.size(); i++)
						{
							std::string nf("tags like '%");
							nf.append(filterparts[0]);
							nf.append("=");
							nf.append(orparts[i]);
							nf.append("%'");

							oss << nf;
							if (i < orparts.size() - 1)
								oss << " or ";
						}

						oss << ")";
					}
					else
					{
						std::string nf("tags like '%");
						nf.append(filterparts[0]);
						nf.append("=");
						nf.append(filterparts[1]);
						nf.append("%'");
						if (oss.str().length() > 0)
							oss << " and ";
						oss << nf;
					}

					sql.where(oss.str());
				}
			}
		}
	
		if (elems.size() >= 3)
			m_Downsampler = elems[2];
	}

	m_Query.assign(sql.str());
	spdlog::debug(m_Query.c_str());
}

Query::~Query(void)
{
}
