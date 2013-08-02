
/*
 * Copyright (C) 2012 - 2013  微蔡 <microcai@fedoraproject.org>
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

#pragma once

#include <iostream>

#include <boost/log/trivial.hpp>
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;

#include <boost/property_tree/json_parser.hpp>
namespace pt = boost::property_tree;

#include <boost/format.hpp>

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include <boost/hash.hpp>

#include "boost/timedcall.hpp"
#include "boost/urlencode.hpp"
#include "boost/consolestr.hpp"

#include "webqq_impl.hpp"
#include "constant.hpp"
#include "lwqq_status.hpp"

namespace webqq{
namespace qqimpl{
namespace detail{

static std::string create_post_data( std::string vfwebqq )
{
	std::string m = boost::str( boost::format( "{\"vfwebqq\":\"%s\"}" ) % vfwebqq );
	return std::string( "r=" ) + boost::url_encode(m);
}

class update_group_list_op : boost::asio::coroutine
{
	void do_fetch()
	{
		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
		m_buffer = boost::make_shared<boost::asio::streambuf>();

		BOOST_LOG_TRIVIAL(debug) << "getting group list";

		/* Create post data: {"h":"hello","vfwebqq":"4354j53h45j34"} */
		std::string postdata = create_post_data(m_webqq->m_vfwebqq);
		std::string url = boost::str(boost::format("%s/api/get_group_name_list_mask2") % "http://s.web2.qq.com");

		m_stream->request_options(
			avhttp::request_opts()
			(avhttp::http_options::request_method, "POST")
			(avhttp::http_options::cookie, m_webqq->m_cookie_mgr.get_cookie(url)())
			(avhttp::http_options::referer, "http://s.web2.qq.com/proxy.html?v=20110412001&callback=1&id=1")
			(avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8")
			(avhttp::http_options::request_body, postdata)
			(avhttp::http_options::content_length, boost::lexical_cast<std::string>(postdata.length()))
			(avhttp::http_options::connection, "close")
		);

		avhttp::async_read_body(*m_stream, url, *m_buffer, *this);
	}

	static void make_update_group_list_op(boost::shared_ptr<WebQQ> webqq, webqq::webqq_handler_t handler)
	{
		update_group_list_op op(webqq, handler);
	}

public:
	update_group_list_op(boost::shared_ptr<WebQQ> webqq, webqq::webqq_handler_t handler)
		: m_webqq(webqq), m_handler(handler)
	{
		do_fetch();
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
 		pt::ptree	jsonobj;
 		std::istream jsondata(m_buffer.get());
 		bool retry = false;

		try{
			js::read_json(jsondata, jsonobj);

			if(jsonobj.get<int>("retcode") == 0)
			{
				BOOST_FOREACH(pt::ptree::value_type result,
								jsonobj.get_child("result.gnamelist"))
				{
					boost::shared_ptr<qqGroup> newgroup = boost::make_shared<qqGroup>();

					newgroup->gid = result.second.get<std::string>("gid");
					newgroup->name = result.second.get<std::string>("name");
					newgroup->code = result.second.get<std::string>("code");

					if(newgroup->gid[0] == '-')
					{
						retry = true;
						BOOST_LOG_TRIVIAL(error) <<  "qqGroup get error" << std::endl;

					}else{
						m_webqq->m_groups.insert(std::make_pair(newgroup->gid, newgroup));
						BOOST_LOG_TRIVIAL(info) << console_out_str("qq群 ") << console_out_str(newgroup->gid) << " " <<  console_out_str(newgroup->name);
					}
				}
			}
		}catch (const pt::ptree_error &){retry = true;}

		if (retry){
			boost::delayedcallsec(m_webqq->get_ioservice(), 15,
					boost::bind(&make_update_group_list_op, m_webqq, m_handler)
			);
			return;
		}

		// and then update_group_detail
		m_webqq->get_ioservice().post(
				boost::asio::detail::bind_handler(*this, boost::system::error_code())
		);
	}

	void operator()(boost::system::error_code ec)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{
			// 先更新群的 QQ 号.
			for (iter = m_webqq->m_groups.begin(); iter != m_webqq->m_groups.end(); ++iter)
			{
				BOOST_ASIO_CORO_YIELD
					m_webqq->update_group_qqnumber(iter->second, *this);
			}

			// 然后更新 Q群的 群友列表.
			for (iter = m_webqq->m_groups.begin(); iter != m_webqq->m_groups.end(); ++iter)
			{
				BOOST_ASIO_CORO_YIELD
					m_webqq->update_group_member(iter->second , *this);
			}
		}

		m_handler(boost::system::error_code());
	}

private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	webqq::webqq_handler_t m_handler;

	read_streamptr m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;

	grouplist::iterator iter;
};

}
}
}
