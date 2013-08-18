
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

template<class Handler>
class update_group_member_op : boost::asio::coroutine
{
public:
	update_group_member_op(boost::shared_ptr<WebQQ> webqq,
		boost::shared_ptr<qqGroup> group, Handler handler)
		: m_webqq(webqq)
		, m_handler(handler)
		, m_buffer(boost::make_shared<boost::asio::streambuf>())
	{
		m_stream.reset(new avhttp::http_stream(m_webqq->get_ioservice()));

		std::string url = boost::str(
			boost::format("%s/api/get_group_info_ext2?gcode=%s&vfwebqq=%s&t=%ld")
				% "http://s.web2.qq.com"
				% group->code
				% m_webqq->m_vfwebqq
				% std::time(NULL)
		);

		m_stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::cookie, m_webqq->m_cookie_mgr.get_cookie(url)() )
			( avhttp::http_options::referer, LWQQ_URL_REFERER_QUN_DETAIL )
			( avhttp::http_options::connection, "close" )
			);

		avhttp::async_read_body(
			*m_stream, url, * m_buffer,
			boost::bind<void>( *this, _1, _2, group)
		);
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered, boost::shared_ptr<qqGroup> group)
	{
		pt::ptree jsonobj;
		std::istream jsondata(m_buffer.get());
		//处理!
		try
		{
			pt::json_parser::read_json(jsondata, jsonobj);

			group_member_process_json(jsonobj, group);

			// 开始更新成员的 QQ 号码，一次更新一个，慢慢来.
			m_webqq->update_group_member_qq(group);

			m_webqq->get_ioservice().post(
				boost::asio::detail::bind_handler(m_handler, boost::system::error_code())
			);
		}
		catch (const pt::ptree_error& jserr)
		{
			BOOST_LOG_TRIVIAL(error) <<  __FILE__ << " : " << __LINE__ << " : "
				<<  "parse json error : " <<  jserr.what();

			m_webqq->get_ioservice().post(
				boost::asio::detail::bind_handler(m_handler, boost::system::error_code())
			);
		}
	}

private:
	void group_member_process_json(pt::ptree& jsonobj, boost::shared_ptr<qqGroup> group)
	{
		//TODO, group members
		if (jsonobj.get<int>("retcode") == 0)
		{
			group->owner = jsonobj.get<std::string>("result.ginfo.owner");

			m_webqq->m_buddy_mgr.set_group_owner(group->gid, group->owner);

			BOOST_FOREACH(pt::ptree::value_type & v, jsonobj.get_child("result.minfo"))
			{
				pt::ptree& minfo = v.second;
				std::string uin = minfo.get<std::string>("uin");
				std::string nick = minfo.get<std::string>("nick");

				m_webqq->m_buddy_mgr.group_new_buddy(group->gid, uin, "", nick);
			}

			BOOST_FOREACH(pt::ptree::value_type & v, jsonobj.get_child("result.ginfo.members"))
			{
				pt::ptree& minfo = v.second;
				std::string muin = minfo.get<std::string>("muin");
				std::string mflag = minfo.get<std::string>("mflag");

				try
				{
					m_webqq->m_buddy_mgr.buddy_update_mflag(muin, boost::lexical_cast<unsigned int>(mflag));
				}
				catch (boost::bad_lexical_cast& e) {}
			}

			try
			{
				BOOST_FOREACH(pt::ptree::value_type & v, jsonobj.get_child("result.cards"))
				{
					pt::ptree& minfo = v.second;
					std::string muin = minfo.get<std::string>("muin");
					std::string card = minfo.get<std::string>("card");

					m_webqq->m_buddy_mgr.buddy_update_card(muin, card);
				}
			}
			catch (const pt::ptree_bad_path& badpath)
			{
			}
		}
	}


private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	Handler m_handler;

	read_streamptr m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;

	grouplist::iterator iter;
};

template<class Handler>
update_group_member_op<Handler>
make_update_group_member_op(boost::shared_ptr<WebQQ> webqq, boost::shared_ptr<qqGroup> group, Handler handler)
{
	return update_group_member_op<Handler>(webqq, group, handler);
}

} // nsmespace detail


template<class Handler>
void update_group_member(boost::shared_ptr<WebQQ> webqq, boost::shared_ptr<qqGroup> group, Handler handler)
{
	detail::make_update_group_member_op(webqq, group, handler);
}

} // nsmespace qqimpl
} // namespace webqq
