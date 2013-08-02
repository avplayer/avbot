
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
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;
#include <boost/make_shared.hpp>

#include <boost/avloop.hpp>

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include <boost/asio/yield.hpp>

#include "boost/timedcall.hpp"
#include "boost/urlencode.hpp"
#include "boost/consolestr.hpp"

#include "webqq_impl.hpp"

#include "constant.hpp"

#include "utf8.hpp"

namespace webqq{
namespace qqimpl{
namespace detail{

struct process_group_message_op : boost::asio::coroutine
{
	process_group_message_op(boost::shared_ptr<WebQQ> webqq, const boost::property_tree::wptree& jstree/*, webqq::webqq_poll_handler_t handler*/)
		: m_jstree(boost::make_shared<boost::property_tree::wptree>(jstree) ), m_webqq(webqq) //, m_handler(handler)
	{
		avloop_idle_post(m_webqq->get_ioservice(), boost::asio::detail::bind_handler(*this, boost::system::error_code(), std::string("") ) );
	}

	void operator()(boost::system::error_code ec, std::string url)
	{
		std::string group_code = wide_utf8( m_jstree->get<std::wstring>( L"value.from_uin" ) );
		std::string who = wide_utf8( m_jstree->get<std::wstring>( L"value.send_uin" ) );

		//parse content

		namespace pt = boost::property_tree;

		pt::wptree::value_type * content;

		reenter(this)
		{
			m_iterator = m_jstree->get_child( L"value.content" ).begin();
			m_iterator_end = m_jstree->get_child( L"value.content" ).end();

			for( ; m_iterator != m_iterator_end; m_iterator ++ )
			{
				content = &(*m_iterator);


				if( content->second.count( L"" ) )
				{
					if( content->second.begin()->second.data() == L"font" )
					{
						msg.type = qqMsg::LWQQ_MSG_FONT;
						msg.font = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"name" ) );
						messagecontent.push_back( msg );
					}
					else if( content->second.begin()->second.data() == L"face" )
					{
						msg.type = qqMsg::LWQQ_MSG_FACE;
						int wface = boost::lexical_cast<int>( content->second.rbegin()->second.data() );
						msg.face = m_webqq->facemap[wface];
						messagecontent.push_back( msg );
					}
					else if( content->second.begin()->second.data() == L"cface" )
					{
						msg.type = qqMsg::LWQQ_MSG_CFACE;
						msg.cface.uin = who;
						msg.cface.gid = m_webqq->get_Group_by_gid( group_code )->code;

						msg.cface.file_id = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"file_id" ) );
						msg.cface.name = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"name" ) );
						msg.cface.vfwebqq = m_webqq->m_vfwebqq;
						msg.cface.key = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"key" ) );
						msg.cface.server = wide_utf8( content->second.rbegin()->second.get<std::wstring> ( L"server" ) );
						msg.cface.cookie = m_webqq->m_cookie_mgr.get_cookie("http://web.qq.com/cgi-bin/get_group_pic")() ;

						BOOST_ASIO_CORO_YIELD
							webqq::async_cface_url_final(m_webqq->get_ioservice(), msg.cface, *this );
						msg.cface.gchatpicurl = url;

						messagecontent.push_back( msg );
					}
				}
				else
				{
					//聊天字符串就在这里.
					msg.type = qqMsg::LWQQ_MSG_TEXT;
					msg.text = wide_utf8( content->second.data() );
					messagecontent.push_back( msg );
				}
			}

			m_webqq->siggroupmessage( group_code, who, messagecontent );
		}

	}

private:
	boost::shared_ptr<WebQQ> m_webqq;
	boost::shared_ptr<boost::property_tree::wptree> m_jstree;
	boost::property_tree::wptree * value_content;
	boost::property_tree::wptree::iterator	m_iterator, m_iterator_end;
	std::vector<qqMsg>	messagecontent;
	qqMsg msg;

};

}
}
} // namespace webqq
