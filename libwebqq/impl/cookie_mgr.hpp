

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

/*
 * 表示一个 cookie 对象,  可以获取用户 cookie: 的字符串,  也可以继续操纵其存储的 cookie
 */
class cookie{
	std::vector<std::pair<std::string, std::string> > m_cookies;
public:

	cookie(std::vector<std::string> names, std::vector<std::string> values)
	{
		BOOST_ASSERT(names.size() == values.size());

		for(int i=0;i<names.size(); i++)
		{
			m_cookies.push_back(std::make_pair(names[i], values[i]));
		}
	}

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
		db <<
			"create table if not exists cookies ("
				"`domain` TEXT not null,"
				"`path` TEXT not null default \"/\", "
				"`name` TEXT not null, "
				"`value` TEXT not null default \"\", "
				"`expiration` TEXT not null default \"session\""
			");";
	}

	// 以 set-cookie: 行的字符串设置 cookie
	void set_cookie(std::string domain, const std::string &set_cookie_line,
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
						v = boost::to_lower_copy(std::string(what[2]));

						// 设置超时时间.
						if ( k == "expires")
						{
							expires = v;
						}else if (k == "path")
						{
							path = v;
						}else if (k == "domain")
						{
							domain = v;
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



				//if (expires!="session")
				if (0)
				{
					// 根据　expires 确定是否为删除操作
					boost::posix_time::time_duration dur =
						boost::posix_time::from_iso_string(expires)
						-
						boost::posix_time::from_time_t(std::time(NULL));

					if (dur.is_negative())
					{
						// 检查是否在　inserted 有了！　如果有了，则删除操作是不可以的。
						if (std::find(inserted.begin(), inserted.end(), boost::make_tuple(domain, path, name)) == inserted.end() )
						{
							// 可删！
							delete_cookie(domain, path, name);
						}
						return;
					}
				}
				// 更新到数据库！
				set_cookie(domain, path, name, value, expires);
				inserted.push_back(boost::make_tuple(domain, path, name));

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
		db.open(soci::sqlite3, dbpath);
		check_db_initialized();
	}

	static std::string build_domain_conditions(std::string domain)
	{
		bool prepend_or = false;
		std::stringstream condition;
		do{
			if (prepend_or)
				condition <<  " or " ;
			condition <<  "domain = \"" << domain << "\" or domain = \"." <<  domain <<  "\" ";
			prepend_or = true;
			if (domain.find_first_of('.')!= std::string::npos)
			{
				domain = domain.substr(domain.find_first_of('.')+1);
			}
		}while (domain.find_first_of('.') != std::string::npos );
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

		std::string sql = boost::str(boost::format("select name, value from cookies where %s") % build_domain_conditions(url.host()) );

		db << sql , soci::into(names, inds_names), soci::into(values, inds_values) ;

		return cookie(names, values);
	}

	// 调用以设置 cookie, 这个是其中一个重载, 用于从 http_stream 获取 set-cookie 头
	void set_cookie(const avhttp::http_stream & stream)
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
				set_cookie(url.host(), v.second, inserted);
			}
		}
	}

	// 以 set-cookie: 行的字符串设置 cookie
	void set_cookie(std::string domain, const std::string &set_cookie_line)
	{
 		std::vector< boost::tuple<std::string, std::string, std::string> > inserted;
 		set_cookie(domain, set_cookie_line, inserted);
	}

	// 详细参数直接设置一个 cookie
	void set_cookie(std::string domain, std::string path, std::string name, std::string value, std::string expiration)
	{
		using namespace soci;
		transaction transac(db);

		db<< "delete from cookies where domain = :domain and path = :path and name = :name" ,  use(domain), use(path), use(name);

		db << "insert into cookies (domain, path, name, value, expiration) values ( :name  , :value  , :domain, :path , :expiration)"
			, use(domain), use(path), use(name), use(value), use(expiration) ;

		transac.commit();
	}

	void delete_cookie(std::string domain, std::string path, std::string name)
	{
		using namespace soci;
		db<< "delete from cookies where domain = :domain and path = :path and name = :name" ,  use(domain), use(path), use(name);
	}
};

}
