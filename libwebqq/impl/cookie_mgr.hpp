

/*
 * Copyright (C) 2013  微蔡 <microcai@fedoraproject.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * cookie manager
 *
 *
 * this is an simple cookie manager class that designed to work with WebQQ in mind.
 *
 * What is a cookie?  A cookie is simply an k/v value with a little bit extra
 * information such as domain and expiries time.
 *
 * With this in mind, I developed an cookie manager for use with avhttp. The cookie
 * is stored individuly, when you access the domain, then the cookie manager collect
 * all cookies needed to access the URI.
 *
 */

#pragma once

#include <string>
#include <ctime>
#include <algorithm>

#include <boost/system/error_code.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include "boost/date_time/gregorian/gregorian.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/date.hpp>
#include <boost/lexical_cast.hpp>

#include <soci-sqlite3.h>
#include <boost-optional.h>
#include <boost-tuple.h>
#include <boost-gregorian-date.h>
#include <soci.h>

#include <avhttp/url.hpp>
#include <avhttp/http_stream.hpp>

namespace cookie{
namespace error{

template<class Error_category>
const boost::system::error_category& error_category_single()
{
	static Error_category error_category_instance;
	return reinterpret_cast<const boost::system::error_category&>(error_category_instance);
}

class error_category_impl;

inline const boost::system::error_category& error_category()
{
	return error_category_single<error_category_impl>();
}

enum errc_t
{
	DATABASE_ERROR = 1,
};

inline boost::system::error_code make_error_code(errc_t e)
{
	return boost::system::error_code(static_cast<int>(e), avhttp::error_category());
}

class error_category_impl
  : public boost::system::error_category
{
	virtual const char* name() const BOOST_SYSTEM_NOEXCEPT
	{
		return "cookie_manager";
	};

	virtual std::string message(int e) const
	{
		switch (e)
		{
			case DATABASE_ERROR:
				return "error in database";
		}
		return "unknow";
	}
};

} // namespace error
} // namespace cookie

namespace boost {
namespace system {

template <>
struct is_error_code_enum<cookie::error::errc_t>
{
  static const bool value = true;
};

} // namespace system
} // namespace boost

namespace cookie{
namespace detail{

/* Portable, consistent toupper (remember EBCDIC). Do not use toupper() because
its behavior is altered by the current locale. */
inline char url_raw_toupper(char in)
{
	if (in >='a' && in<='z')
		return in & 0x5F;
	return in;
}

inline bool url_raw_equal(const char *first, const char *second)
{
	while(*first && *second)
	{
		if(url_raw_toupper(*first) != url_raw_toupper(*second))
			/* get out of the loop as soon as they don't match */
			break;

		first++;
		second++;
	}

	/* we do the comparison here (possibly again), just to make sure that if the
	loop above is skipped because one of the strings reached zero, we must not
	return this as a successful match */
	return (url_raw_toupper(*first) == url_raw_toupper(*second));
}

inline int checkmonth(const char *check)
{
	static const char * const Curl_month[] =
	{
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	int i;
	const char * const *what;
	bool found = false;

	what = &Curl_month[0];

	for(i = 0; i < 12; i++)
	{
		if(url_raw_equal(check, what[0]))
		{
			found = true;
			break;
		}

		what++;
	}

	return found ? i : -1; /* return the offset or -1, no real offset is -1 */
}

/* return the time zone offset between GMT and the input one, in number
of seconds or -1 if the timezone wasn't found/legal */
inline int checktz(const char *check)
{
	#define tDAYZONE -60 /* offset for daylight savings time */

	struct tzinfo
	{
		char name[5];
		int offset; /* +/- in minutes */
	} tz [] =
	{
		{"GMT", 0}, /* Greenwich Mean */
		{"UTC", 0}, /* Universal (Coordinated) */
		{"WET", 0}, /* Western European */
		{"BST", 0 tDAYZONE}, /* British Summer */
		{"WAT", 60}, /* West Africa */
		{"AST", 240}, /* Atlantic Standard */
		{"ADT", 240 tDAYZONE}, /* Atlantic Daylight */
		{"EST", 300}, /* Eastern Standard */
		{"EDT", 300 tDAYZONE}, /* Eastern Daylight */
		{"CST", 360}, /* Central Standard */
		{"CDT", 360 tDAYZONE}, /* Central Daylight */
		{"MST", 420}, /* Mountain Standard */
		{"MDT", 420 tDAYZONE}, /* Mountain Daylight */
		{"PST", 480}, /* Pacific Standard */
		{"PDT", 480 tDAYZONE}, /* Pacific Daylight */
		{"YST", 540}, /* Yukon Standard */
		{"YDT", 540 tDAYZONE}, /* Yukon Daylight */
		{"HST", 600}, /* Hawaii Standard */
		{"HDT", 600 tDAYZONE}, /* Hawaii Daylight */
		{"CAT", 600}, /* Central Alaska */
		{"AHST", 600}, /* Alaska-Hawaii Standard */
		{"NT", 660}, /* Nome */
		{"IDLW", 720}, /* International Date Line West */
		{"CET", -60}, /* Central European */
		{"MET", -60}, /* Middle European */
		{"MEWT", -60}, /* Middle European Winter */
		{"MEST", -60 tDAYZONE}, /* Middle European Summer */
		{"CEST", -60 tDAYZONE}, /* Central European Summer */
		{"MESZ", -60 tDAYZONE}, /* Middle European Summer */
		{"FWT", -60}, /* French Winter */
		{"FST", -60 tDAYZONE}, /* French Summer */
		{"EET", -120}, /* Eastern Europe, USSR Zone 1 */
		{"WAST", -420}, /* West Australian Standard */
		{"WADT", -420 tDAYZONE}, /* West Australian Daylight */
		{"CCT", -480}, /* China Coast, USSR Zone 7 */
		{"JST", -540}, /* Japan Standard, USSR Zone 8 */
		{"EAST", -600}, /* Eastern Australian Standard */
		{"EADT", -600 tDAYZONE}, /* Eastern Australian Daylight */
		{"GST", -600}, /* Guam Standard, USSR Zone 9 */
		{"NZT", -720}, /* New Zealand */
		{"NZST", -720}, /* New Zealand Standard */
		{"NZDT", -720 tDAYZONE}, /* New Zealand Daylight */
		{"IDLE", -720}, /* International Date Line East */
		/* Next up: Military timezone names. RFC822 allowed these, but (as noted in
		RFC 1123) had their signs wrong. Here we use the correct signs to match
		actual military usage.
		*/
		{"A", +1 * 60}, /* Alpha */
		{"B", +2 * 60}, /* Bravo */
		{"C", +3 * 60}, /* Charlie */
		{"D", +4 * 60}, /* Delta */
		{"E", +5 * 60}, /* Echo */
		{"F", +6 * 60}, /* Foxtrot */
		{"G", +7 * 60}, /* Golf */
		{"H", +8 * 60}, /* Hotel */
		{"I", +9 * 60}, /* India */
		/* "J", Juliet is not used as a timezone, to indicate the observer's local
		time */
		{"K", +10 * 60}, /* Kilo */
		{"L", +11 * 60}, /* Lima */
		{"M", +12 * 60}, /* Mike */
		{"N", -1 * 60}, /* November */
		{"O", -2 * 60}, /* Oscar */
		{"P", -3 * 60}, /* Papa */
		{"Q", -4 * 60}, /* Quebec */
		{"R", -5 * 60}, /* Romeo */
		{"S", -6 * 60}, /* Sierra */
		{"T", -7 * 60}, /* Tango */
		{"U", -8 * 60}, /* Uniform */
		{"V", -9 * 60}, /* Victor */
		{"W", -10 * 60}, /* Whiskey */
		{"X", -11 * 60}, /* X-ray */
		{"Y", -12 * 60}, /* Yankee */
		{"Z", 0}, /* Zulu, zero meridian, a.k.a. UTC */
	};

#undef tDAYZONE

	unsigned int i;
	const tzinfo *what;
	bool found = false;

	what = tz;

	for(i = 0; i < sizeof(tz) / sizeof(tz[0]); i++)
	{
		if(url_raw_equal(check, what->name))
		{
			found = true;
			break;
		}

		what++;
	}

	return found ? what->offset * 60 : -1;
}

inline boost::posix_time::ptime ptime_from_gmt_string(std::string date)
{
	boost::smatch what;
	/*
	 * time is of format
	 *   Thu, 01 Jan 1970 00:00:01 GMT
     */

	if (boost::regex_search(date, what, boost::regex("([A-z]+), +([\\d]+)[ -]+([a-zA-Z0-9]+)[ -]+([\\d]+) +([0-9][0-9]):([0-9][0-9]):([0-9][0-9]) +([a-zA-Z0-9\\+]+)")))
	{
		// 放弃解析星期， 呵呵

		long year, mouth, day;

		long hour, min, sec;

		year = boost::lexical_cast<long>(what[4]);
		mouth =  checkmonth( std::string(what[3]).c_str() ) + 1;
		day = boost::lexical_cast<long>(2);

		hour = boost::lexical_cast<long>(what[5]);
		min = boost::lexical_cast<long>(what[6]);
		sec = boost::lexical_cast<long>(what[7]);

		long tzoff = checktz(std::string(what[8]).c_str());

		return

		boost::posix_time::ptime(boost::gregorian::date(year, mouth, day))
		  +
		boost::posix_time::hours(hour)
		  +
		boost::posix_time::minutes(min)
		  +
		boost::posix_time::seconds(sec)
		  +
		boost::posix_time::seconds(tzoff);
	}

	return boost::posix_time::from_time_t(0);
}

}

class cookie_store;

/*
 * 表示一个 cookie 对象,  可以获取用户 cookie: 的字符串,  也可以继续操纵其存储的 cookie
 */
class cookie{
	std::vector<std::pair<std::string, std::string> > m_cookies;

	cookie(std::vector<std::string> names, std::vector<std::string> values)
	{
		BOOST_ASSERT(names.size() == values.size());

		for(int i=0;i<names.size(); i++)
		{
			m_cookies.push_back(std::make_pair(names[i], values[i]));
		}
	}
	friend class cookie_store;
public:

	operator bool()
	{
		return !m_cookies.empty();
	}

	// 返回 cookie: 所应该使用的字符串.
	std::string operator()()
	{
		if (m_cookies.empty())
		{
			return "null=null";
		}
		std::stringstream outline;

		for (int i =0; i < m_cookies.size(); i++)
			outline << m_cookies[i].first << "=" << m_cookies[i].second << "; ";

		return outline.str();
	}

	std::string get_value(std::string name)
	{
		for (int i =0; i < m_cookies.size(); i++)
			if (m_cookies[i].first == name)
				return  m_cookies[i].second;
		return "";
	}
};

/* 管理和存储 cookie, WebQQ 最重要的任务就是保护这么一个对象*/
class cookie_store : boost::noncopyable
{
	soci::session db;

	void check_db_initialized()
	{
		soci::transaction trans(db);
		db <<
			"create table if not exists cookies ("
				"`domain` TEXT not null,"
				"`path` TEXT not null default \"/\", "
				"`name` TEXT not null, "
				"`value` TEXT not null default \"\", "
				"`expiration` TEXT not null default \"session\""
			");";
		trans.commit();
	}

	// 以 set-cookie: 行的字符串设置 cookie
	void save_cookie(std::string domain, const std::string &set_cookie_line,
			std::vector< boost::tuple<std::string, std::string, std::string> > & inserted)
	{
		std::string name, value, path = "/", expires = "session";

		// 调用　set_cookie
		std::vector<std::string> tokens;

		if (boost::split(tokens, set_cookie_line, boost::is_any_of(";")).size()>1)
		{
			BOOST_FOREACH(std::string & t, tokens){boost::trim_left(t);}

			std::string cookie = tokens[0];

			boost::smatch what;
			boost::regex ex("^(.+)=(.*)$");
			if ( boost::regex_match(cookie, what, ex))
			{
				name = what[1];
				value = what[2];

				// 接下来是根据 Expires
				for(int i=1;i < tokens.size();i++)
				{
					std::string token = tokens[i];

					if (boost::regex_match(token, what, ex))
					{
						std::string k;
						std::string v;

						k = boost::to_lower_copy(std::string(what[1]));
						v = what[2];

						// 设置超时时间.
						if ( k == "expires")
						{
							expires = v;
						}else if (k == "path")
						{
							path = v;
						}else if (k == "domain")
						{
							domain = boost::to_lower_copy(v);
						}else if (k == "max-age")
						{
							// set expires with age
							expires =  boost::posix_time::to_iso_string(
		   						boost::posix_time::from_time_t(std::time(NULL)) +
								boost::posix_time::seconds(boost::lexical_cast<long>(v))
							);
						}
					}
				}

				bool is_delete = value.empty();

				if ( !is_delete && expires!="session")
				{
					// 根据　expires 确定是否为删除操作
					boost::posix_time::time_duration dur =
						detail::ptime_from_gmt_string(expires)
						-
						boost::posix_time::from_time_t(std::time(NULL));

					is_delete = dur.is_negative();
				}

				if (is_delete)
				{
					// 检查是否在　inserted 有了！　如果有了，则删除操作是不可以的。
					if (std::find(inserted.begin(), inserted.end(), boost::make_tuple(domain, path, name)) == inserted.end() )
					{
						// 可删！
						delete_cookie(domain, path, name);
					}
				}else{
					// 更新到数据库！
					save_cookie(domain, path, name, value, expires);
					inserted.push_back(boost::make_tuple(domain, path, name));
				}
			}
		}
	}

public:
	// drop session cookies with the domain name,
	// if "" then drop session cookies of all domains
	void drop_session(const std::string & domain = std::string())
	{
		// TODO check for % and \" \' charactor
		//db << "delete from cookies";

 		std::string sqlstmt =
 			boost::str(boost::format("delete from cookies where expiration = \"session\" and domain like \"%%%s\"") % domain);
		db <<  sqlstmt;
	}

 	cookie_store(const std::string & dbpath = std::string(":memory:"))
	{
		sqlite_api::sqlite3_enable_shared_cache(1);
		db.open(soci::sqlite3, dbpath);
		check_db_initialized();
	}

	static std::string build_domain_conditions(std::string domain)
	{
		bool prepend_gama = false;
		std::stringstream condition;

		condition << "domain in (";
		do{
			if (prepend_gama)
				condition <<  " ,  " ;
			condition <<  "\"" << domain << "\" , \"." <<  domain <<  "\" ";
			prepend_gama = true;
			if (domain.find_first_of('.')!= std::string::npos)
			{
				domain = domain.substr(domain.find_first_of('.')+1);
			}
		}while (domain.find_first_of('.') != std::string::npos );
		condition << ")";
		return condition.str();
	}

	static std::string build_path_conditions(std::string path)
	{
		bool prepend_gama = false;
		std::stringstream condition;

		// 砍掉 ?
		std::size_t pos = path.find_first_of('?');
		if (pos != std::string::npos){
			path = path.substr(0, pos);
		}

		condition << "path in ( \"/\"";
		do{
			condition <<  ", \"" << path << "\"";
			if (path.find_last_of('/')!= std::string::npos)
			{
				path = path.substr(0, path.find_last_of('/'));
			}
		}while (path.find_last_of('/') != std::string::npos );
		condition << ")";
		return condition.str();
	}

	// 以 url 对象为参数调用就可以获得这个请求应该带上的 cookie
	//
	cookie get_cookie(const avhttp::url & url)
	{
		// 遍历 cookie store, 找到符合 PATH 和 domain 要求的 cookie
		// 然后构建 cookie 对象.

		std::vector<std::string> names(100);
		std::vector<std::string> values(100);

		std::vector<soci::indicator> inds_names;
		std::vector<soci::indicator> inds_values;

		std::string sql = boost::str(
			boost::format("select name, value from cookies where %s and %s")
				% build_domain_conditions(url.host())
				% build_path_conditions(url.path())
		);

		db << sql , soci::into(names, inds_names), soci::into(values, inds_values) ;

		return cookie(names, values);
	}

	// 直接设置 cookie
	void get_cookie(const avhttp::url & url, avhttp::http_stream & avstream)
	{
		avhttp::request_opts opts = avstream.request_options();

		opts.remove(avhttp::http_options::cookie);

		opts.insert(avhttp::http_options::cookie,
			get_cookie(avstream.final_url())()
		);
		avstream.request_options(opts);
	}


	// 直接设置 cookie
	void get_cookie(avhttp::http_stream & avstream)
	{
		get_cookie(avstream.final_url(), avstream);
	}

	// 调用以设置 cookie, 这个是其中一个重载, 用于从 http_stream 获取 set-cookie 头
	void save_cookie(const avhttp::http_stream & stream)
	{
		avhttp::url url(stream.final_url());
		avhttp::option::option_item_list opts = stream.response_options().option_all();

		std::vector< boost::tuple<std::string, std::string, std::string> > inserted;
		// process in reverse order
		BOOST_FOREACH(avhttp::option::option_item v, opts)
		{
			// 寻找 set-cookie
			if (boost::algorithm::to_lower_copy(v.first) == "set-cookie")
			{
				save_cookie(url.host(), v.second, inserted);
			}
		}
	}

	// 以 set-cookie: 行的字符串设置 cookie
	void save_cookie(std::string domain, const std::string &set_cookie_line)
	{
 		std::vector< boost::tuple<std::string, std::string, std::string> > inserted;
 		save_cookie(domain, set_cookie_line, inserted);
	}

	// 详细参数直接设置一个 cookie
	void save_cookie(std::string domain, std::string path, std::string name, std::string value, std::string expiration)
	{
		using namespace soci;
		transaction transac(db);

		db<< "delete from cookies where domain = :domain and path = :path and name = :name"
			, use(domain), use(path), use(name);

		db << "insert into cookies (domain, path, name, value, expiration) values ( :name  , :value  , :domain, :path , :expiration)"
			, use(domain), use(path), use(name), use(value), use(expiration) ;

		transac.commit();
	}

	void delete_cookie(std::string domain, std::string path, std::string name)
	{
		using namespace soci;
		db<< "delete from cookies where domain = :domain and path = :path and name = :name"
		  , use(domain), use(path), use(name);
	}
};

}
