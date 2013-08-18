
/*
 *
 */


#pragma once

#include <boost/log/trivial.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;

#include <avhttp/async_read_body.hpp>

#include "boost/timedcall.hpp"
#include "boost/consolestr.hpp"
#include "boost/urlencode.hpp"

#include "webqq_impl.hpp"

#include "constant.hpp"
#include "utf8.hpp"

#include "process_group_msg.hpp"

namespace webqq{
namespace qqimpl{
namespace detail{

template<class Handler>
class poll_message_op:boost::asio::coroutine
{
public:
	poll_message_op(boost::shared_ptr<qqimpl::WebQQ> webqq,  Handler handler)
		: m_webqq(webqq), m_handler(handler)
	{
		/* Create a POST request */
		std::string msg = boost::str(
							boost::format( "{\"clientid\":\"%s\",\"psessionid\":\"%s\"}" )
							% m_webqq->m_clientid
							% m_webqq->m_psessionid
						);

		msg = boost::str( boost::format( "r=%s\r\n" ) %  boost::url_encode(msg) );

		m_buffer = boost::make_shared<boost::asio::streambuf>();
		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref( m_webqq->get_ioservice()));

		m_stream->request_options( avhttp::request_opts()
									( avhttp::http_options::request_method, "POST" )
									( avhttp::http_options::cookie, m_webqq->m_cookie_mgr.get_cookie(LWQQ_URL_POLL_MESSAGE)() )
									( "cookie2", "$Version=1" )
									( avhttp::http_options::referer, "http://d.web2.qq.com/proxy.html?v=20101025002" )
									( avhttp::http_options::request_body, msg )
									( avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8" )
									( avhttp::http_options::content_length, boost::lexical_cast<std::string>( msg.length() ) )
									( avhttp::http_options::connection, "close" )
								);

		avhttp::async_read_body( *m_stream, LWQQ_URL_POLL_MESSAGE, * m_buffer, *this);
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
		if ( ec ){
			// 出现网络错误
			m_webqq->get_ioservice().post(
				boost::asio::detail::bind_handler(
					m_handler,
					error::make_error_code(error::poll_failed_network_error)
				)
			);
			return;
		}

		m_jstree = boost::make_shared<pt::wptree>();

		std::wstring response = utf8_wide(
			std::string(
				boost::asio::buffer_cast<const char*>(m_buffer->data()) , m_buffer->size()
			)
		);


		std::wstringstream jsondata;
		jsondata << response;

		int retcode;

		ec =  boost::system::error_code();

		//处理!
		try
		{
			pt::json_parser::read_json( jsondata, *m_jstree );

			retcode = m_jstree->get<int>( L"retcode" );

			if(retcode == 116)
			{
				// update ptwebqq
				std::string ptwebqq = wide_utf8(m_jstree->get<std::wstring>( L"p"));
				m_webqq->m_cookie_mgr.save_cookie("qq.com", "/", "ptwebqq", ptwebqq, "session");
				ec =  boost::system::error_code();
			}else if(retcode == 102)
			{
			}
			else if(retcode == 121 || retcode == 120 || retcode == 100)
			{
				// 需要认证
				ec = error::make_error_code(error::poll_failed_need_login);
			}else if(retcode == 103)
			{
				ec = error::make_error_code(error::poll_failed_need_login);
			}else if (retcode == 110)
			{
				ec = error::make_error_code(error::poll_failed_user_quit);
			}else if (retcode){
				ec = error::make_error_code(error::poll_failed_unknow_ret_code);
			}else
			{
				retcode = 0;
			}
		}
		catch( const pt::ptree_error & badpath )
		{
			BOOST_LOG_TRIVIAL(error) <<  __FILE__ << " : " << __LINE__ << " : " <<  "bad path" <<  badpath.what();
			// 出现网络错误
			m_webqq->get_ioservice().post(
				boost::asio::detail::bind_handler(
					m_handler,
					error::make_error_code(error::poll_failed_network_error)
				)
			);
			return;
		}

		if (retcode)
		{
			return m_webqq->get_ioservice().post(
				boost::bind<void>(m_handler, ec)
			);
		}

		m_webqq->get_ioservice().post(
			boost::bind<void>(*this, boost::system::error_code())
		);
	}

	void operator()(boost::system::error_code ec)
	{
		std::string poll_type;
 		pt::wptree::value_type * result;

		BOOST_ASIO_CORO_REENTER(this)
		{

			m_iterator = m_jstree->get_child( L"result" ).begin();
			m_iterator_end = m_jstree->get_child( L"result" ).end();

			for( ; m_iterator != m_iterator_end; m_iterator ++ )
			{
				result = &(*m_iterator);

				poll_type = wide_utf8( result->second.get<std::wstring>( L"poll_type" ) );

				if (poll_type != "group_message")
				{
					js::write_json( std::wcout, *m_jstree );
				}
				else if( poll_type == "group_message" )
				{
					BOOST_ASIO_CORO_YIELD process_group_message(
						m_webqq, result->second,
						*this
					);
				}
				else if( poll_type == "sys_g_msg" )
				{
					//群消息.
					if( result->second.get<std::wstring>( L"value.type" ) == L"group_join" )
					{
						// 新人进来 !
						// 检查一下新人.
						// 这个是群号.
						std::string groupnumber = wide_utf8(
							result->second.get<std::wstring>(L"value.t_gcode")
						);

						std::string newuseruid = wide_utf8(
							result->second.get<std::wstring>(L"value.new_member")
						);

						qqGroup_ptr group = m_webqq->get_Group_by_gid(groupnumber);

						// 报告一下新人入群!
						m_webqq->m_group_refresh_queue.push(
							boost::make_tuple(
								// callback
								boost::bind(&WebQQ::cb_newbee_group_join, m_webqq, group, newuseruid),
								groupnumber
							)
						);
					}else if(result->second.get<std::wstring>( L"value.type" ) == L"group_leave")
					{
						// 旧人滚蛋.
					}
				} else if( poll_type == "buddylist_change" ) {
					//群列表变化了，reload列表.
					js::write_json( std::wcout, result->second );
				} else if( poll_type == "kick_message" ) {
					js::write_json( std::wcout, result->second );
					//强制下线了，重登录.
					ec = error::make_error_code(error::poll_failed_user_kicked_off);
				}
			}

			return m_webqq->get_ioservice().post(
				boost::bind<void>(m_handler, ec)
			);
		}
	}

private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	Handler m_handler;
	boost::shared_ptr<avhttp::http_stream> m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;

	//
	boost::shared_ptr<pt::wptree> m_jstree;
	boost::property_tree::wptree::iterator m_iterator, m_iterator_end;
};

}

template<class Handler>
detail::poll_message_op<Handler> poll_message(boost::shared_ptr<WebQQ> webqq, Handler handler)
{
	return detail::poll_message_op<Handler>(webqq, handler);
}

}
} // namespace webqq
