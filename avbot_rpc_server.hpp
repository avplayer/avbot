/*
 * Copyright (C) 2013  microcai <microcai@fedoraproject.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <boost/property_tree/ptree.hpp>
namespace pt = boost::property_tree;
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;
#include <boost/circular_buffer.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/async_coro_queue.hpp>

#include "avhttpd.hpp"

#include "botctl.hpp"

#include <boost/asio/yield.hpp>

namespace detail
{

// avbot_rpc_server 由 acceptor_server 这个辅助类调用
// 为其构造函数传入一个 m_socket, 是 shared_ptr 的.
class avbot_rpc_server
	: boost::asio::coroutine
	, public boost::enable_shared_from_this<avbot_rpc_server>
{
public:
	typedef boost::signals2::signal <
	void( boost::property_tree::ptree )
	> on_message_signal_type;

	on_message_signal_type &broadcast_message;

	typedef boost::asio::ip::tcp Protocol;
	typedef boost::asio::basic_stream_socket<Protocol> socket_type;

	template<typename T>
	avbot_rpc_server( boost::shared_ptr<socket_type> _socket,
		on_message_signal_type & on_message, T do_search_func)
		: m_socket( _socket )
		, m_streambuf( new boost::asio::streambuf )
		, m_responses(boost::ref(_socket->get_io_service()), 20)
		, broadcast_message(on_message)
		, do_search(do_search_func)
	{
	}

	void start()
	{
		avloop_idle_post(m_socket->get_io_service(),
			boost::bind<void>(&avbot_rpc_server::client_loop, shared_from_this(),
					boost::system::error_code(), 0 )
		);
		m_connect = broadcast_message.connect(boost::bind<void>(&avbot_rpc_server::callback_message, this, _1));
	}
private:
	void get_response_sended(boost::shared_ptr< boost::asio::streambuf > v, boost::system::error_code ec, std::size_t);
	void on_pop(boost::shared_ptr<boost::asio::streambuf> v);

	// 循环处理客户端连接.
	void client_loop(boost::system::error_code ec, std::size_t bytestransfered);

	// signal 的回调到这里
	void callback_message(const boost::property_tree::ptree & jsonmessage );
	void done_search(boost::system::error_code ec, boost::property_tree::ptree);
private:
	boost::shared_ptr<socket_type> m_socket;

	boost::signals2::scoped_connection m_connect;

	boost::shared_ptr<boost::asio::streambuf> m_streambuf;
	avhttpd::request_opts m_request;

	boost::async_coro_queue<
		boost::circular_buffer_space_optimized<
			boost::shared_ptr<boost::asio::streambuf>
		>
	> m_responses;

	boost::function<void(
		std::string c,
		std::string q,
		std::string date,
		boost::function<void (boost::system::error_code, pt::ptree)> cb
	)> do_search;

	int process_post( std::size_t bytestransfered );
};
}
