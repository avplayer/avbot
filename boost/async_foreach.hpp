
#pragma once

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/avloop.hpp>

namespace boost{
namespace detail{

template<class InputIterator, class Pred, class Handler>
class async_foreach_op : asio::coroutine
{
	asio::io_service & m_io_service;
	InputIterator	m_first, m_last, m_current;
	Pred _pred;
	Handler m_handler;
public:
	typedef void result_type;

	async_foreach_op(asio::io_service & io_service, InputIterator first,  InputIterator last, Pred pred, Handler handler)
		: m_io_service(io_service), m_first(first), m_last(last), _pred(pred), m_handler(handler)
	{
		avloop_idle_post(m_io_service, asio::detail::bind_handler(*this, boost::system::error_code()));
	}

	void operator()(system::error_code ec)
	{
		// 好了，每次回调检查一个文件，这样才好，对吧.
		BOOST_ASIO_CORO_REENTER(this)
		{
			for( m_current = m_first; m_current != m_last ; ++m_current)
			{
				// 好，处理 dir_it_cur dir_it_;
				BOOST_ASIO_CORO_YIELD
					_pred(*m_current, m_io_service.wrap(bind(*this, _1)));

				BOOST_ASIO_CORO_YIELD
					avloop_idle_post(m_io_service, asio::detail::bind_handler(*this, ec));
			}

			avloop_idle_post(m_io_service, asio::detail::bind_handler(m_handler, ec));
		}
	}

};

template<class InputIterator, class Pred, class Handler>
async_foreach_op<InputIterator, Pred, Handler>
make_async_foreach_op(asio::io_service &io_service, InputIterator first, InputIterator last , Pred pred, Handler handler)
{
	return async_foreach_op<InputIterator, Pred, Handler>(io_service, first, last, pred, handler);
}

} // namespace detail

template<class InputIterator, class Pred, class Handler>
void async_foreach(asio::io_service &io_service, InputIterator first, InputIterator last , Pred pred, Handler handler)
{
	detail::make_async_foreach_op(io_service, first, last, pred, handler);
}

} // namespace boost
