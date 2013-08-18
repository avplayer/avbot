
#pragma once

#include <boost/log/trivial.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;
#include <boost/asio/yield.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

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

class webqq_keepalive_op : boost::asio::coroutine
{
private:
	void start_poll()
	{
		// 搞一个 GET 的长维护
		std::string url = "http://webqq.qq.com/web2/get_msg_tip?uin=&tp=1&id=0"
			"&retype=1&rc=1&lv=3&t=1348458711542";

		m_buffer = boost::make_shared<boost::asio::streambuf>();
		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));

		m_stream->request_options(avhttp::request_opts()
			(avhttp::http_options::cookie, m_webqq->m_cookie_mgr.get_cookie(url)())
			(avhttp::http_options::referer, "http://d.web2.qq.com/proxy.html?v=20101025002")
			(avhttp::http_options::connection, "close")
		);


		avhttp::async_read_body(*m_stream, url, * m_buffer, *this);
	}
public:
	webqq_keepalive_op(boost::shared_ptr<qqimpl::WebQQ> webqq)
		: m_webqq(webqq)
	{
		start_poll();
	}

	void operator()( const boost::system::error_code& ec, std::size_t bytes_transfered)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{for (;m_webqq->m_status!= LWQQ_STATUS_QUITTING;){
			// 清理完成!
			BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
				m_webqq->get_ioservice(), 20,
				boost::bind<void>(*this, ec, bytes_transfered)
			);

			// 重启清理协程.
			BOOST_ASIO_CORO_YIELD start_poll();
		}}
	}
private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	boost::shared_ptr<avhttp::http_stream> m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;
};

webqq_keepalive_op make_keepalive_op(boost::shared_ptr<qqimpl::WebQQ> webqq)
{
	return webqq_keepalive_op(webqq);
}

} // nsmespace detail

void webqq_keepalive(boost::shared_ptr<qqimpl::WebQQ> webqq)
{
	detail::make_keepalive_op(webqq);
}

} // nsmespace qqimpl
} // namespace webqq