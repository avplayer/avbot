
/*
 *
 */


#pragma once

#include <boost/log/trivial.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/regex/pending/unicode_iterator.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <avhttp/async_read_body.hpp>

#include "boost/timedcall.hpp"
#include "boost/urlencode.hpp"

#include "../webqq.hpp"

#include "webqq_impl.hpp"

#include "constant.hpp"
#include "boost/consolestr.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

// 把 boost::u8_to_u32_iterator 封装一下，提供 * 解引用操作符.
class u8_u32_iterator: public boost::u8_to_u32_iterator<std::string::const_iterator>
{
public:
	typedef boost::uint32_t reference;

	reference operator* () const
	{
		return dereference();
	}
	u8_u32_iterator( std::string::const_iterator b ):
		boost::u8_to_u32_iterator<std::string::const_iterator>( b ) {}
};

// 向后迭代，然后返回每个字符
template<class BaseIterator>
struct escape_iterator
{
	BaseIterator m_position;
	typedef std::string reference;

	escape_iterator( BaseIterator b ): m_position( b ) {}

	void operator ++()
	{
		++m_position;
	}

	void operator ++(int)
	{
		++m_position;
	}

	reference operator* () const
	{
		char buf[8] = {0};

		snprintf( buf, sizeof( buf ), "\\\\u%04X", ( boost::uint32_t )( * m_position ) );
		// 好，解引用！
		// 获得 代码点后，就是构造  \\\\uXXXX 了
		return buf;
	}

	bool operator == ( const escape_iterator & rhs ) const
	{
		return m_position == rhs.m_position;
	}

	bool operator != ( const escape_iterator & rhs ) const
	{
		return m_position != rhs.m_position;
	}
};

class group_message_sender_op:boost::asio::coroutine
{
public:
	group_message_sender_op(boost::shared_ptr<qqimpl::WebQQ> webqq)
		: m_webqq(webqq)
	{
		// 进入循环吧.
		webqq->m_group_message_queue.async_pop(
			boost::bind<void>(*this, _1, 0, _2)
		);
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered, boost::tuple<std::string, std::string, WebQQ::send_group_message_cb> v)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{for (;m_webqq->m_status != LWQQ_STATUS_QUITTING;){
			// 等待状态为登录.
			while(m_webqq->m_status != LWQQ_STATUS_ONLINE || m_webqq->m_psessionid.empty())
			{
				BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
						m_webqq->get_ioservice(),
						20,
						boost::asio::detail::bind_handler(*this, ec, bytes_transfered, v)
				);
			}

			while(m_webqq->m_status == LWQQ_STATUS_ONLINE)
			{
				// 开始发送.
				BOOST_ASIO_CORO_YIELD send_group_message(v);

				m_stream.reset();
				m_buffer.reset();

				m_webqq->get_ioservice().post(
					boost::asio::detail::bind_handler(v.get<2>(), ec)
				);

				// 发送完毕， 延时 400ms 再发送
				BOOST_ASIO_CORO_YIELD boost::delayedcallms(
						m_webqq->get_ioservice(),
						400,
						boost::asio::detail::bind_handler(*this, ec, bytes_transfered, v)
				);

				BOOST_ASIO_CORO_YIELD m_webqq->m_group_message_queue.async_pop(
					boost::bind<void>(*this, _1, 0, _2)
				);
			}
		}
		}
	}

private:
	void send_group_message(boost::tuple<std::string, std::string, WebQQ::send_group_message_cb> v)
	{

		std::string group = v.get<0>();
		std::string msg = v.get<1>();

		//unescape for POST
		std::string messagejson = boost::str(
			boost::format("{\"group_uin\":\"%s\", "
				"\"content\":\"["
				"\\\"%s\\\","
				"[\\\"font\\\",{\\\"name\\\":\\\"宋体\\\",\\\"size\\\":\\\"9\\\",\\\"style\\\":[0,0,0],\\\"color\\\":\\\"000000\\\"}]"
				"]\","
				"\"msg_id\":%ld,"
				"\"clientid\":\"%s\","
				"\"psessionid\":\"%s\"}")
				% group
				% parse_unescape( msg )
				% m_webqq->m_msg_id ++
				% m_webqq->m_clientid
				% m_webqq->m_psessionid
		);

		std::string postdata =  boost::str(
				boost::format( "r=%s&clientid=%s&psessionid=%s" )
				% boost::url_encode(messagejson)
				% m_webqq->m_clientid
				% m_webqq->m_psessionid
				);
		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
		m_buffer = boost::make_shared<boost::asio::streambuf>();

		m_stream->request_options(
			avhttp::request_opts()
				( avhttp::http_options::request_method, "POST" )
				( avhttp::http_options::cookie, m_webqq->m_cookie_mgr.get_cookie(LWQQ_URL_SEND_QUN_MSG)() )
				( avhttp::http_options::referer, "http://d.web2.qq.com/proxy.html?v=20110331002&callback=1&id=2" )
				( avhttp::http_options::content_type, "application/x-www-form-urlencoded; charset=UTF-8" )
				( avhttp::http_options::request_body, postdata )
				( avhttp::http_options::content_length, boost::lexical_cast<std::string>( postdata.length() ) )
				( avhttp::http_options::connection, "close" )
				("Origin", "http://d.web2.qq.com")
				("Accept", "*/*")
		);

		avhttp::async_read_body(
			*m_stream,
			LWQQ_URL_SEND_QUN_MSG,
			*m_buffer,
			boost::bind<void>(*this, _1, _2, v)
		);
	}
private:

	static std::string parse_unescape( const std::string & source )
	{
		std::string result;
		escape_iterator<u8_u32_iterator> ues( source.begin() );
		escape_iterator<u8_u32_iterator> end( source.end() );
		try{
			while( ues != end )
			{
				result += * ues;
				++ ues;
			}
		}catch (const std::out_of_range &e)
		{
			BOOST_LOG_TRIVIAL(error) << __FILE__ <<  __LINE__<<  " "  <<  console_out_str("QQ消息字符串包含非法字符 ");
			result += "broken encode sended";
		}

		return result;
	}
private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	boost::shared_ptr<avhttp::http_stream> m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;
};

}

template<class Webqq>
detail::group_message_sender_op group_message_sender(boost::shared_ptr<Webqq> webqq)
{
	return detail::group_message_sender_op(webqq);
}

}
} // namespace webqq
