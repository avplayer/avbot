/*
 * <one line to give the program's name and a brief idea of what it does.>
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
#include <boost/json_create_escapes_utf8.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

#include <avhttp/detail/parsers.hpp>

#include "boost/avloop.hpp"
// #include <boost/asio/yield.hpp>

#include "avbot_rpc_server.hpp"

namespace detail{

/**
 * avbot rpc 接受的 JSON 格式为
 *
 * 	{
		"protocol":"rpc",
		"channel":"",  // 留空表示所有频道广播
		"message":{
			"text" : "text message"
		}
	}
 */
int avbot_rpc_server::process_post( std::size_t bytes_transfered )
{
	pt::ptree msg;
	std::string messagebody;
	messagebody.resize( bytes_transfered );
	m_streambuf->sgetn( &messagebody[0], bytes_transfered );
	std::stringstream jsonpostdata( messagebody );

	try
	{
		// 读取 json
		js::read_json( jsonpostdata, msg );
	}
	catch( const js::json_parser_error & err )
	{
		// 数据不是 json 格式，视作 纯 TEXT 格式.
		msg.put( "protocol", "rpc" );
		msg.put( "channel", "" );
		msg.put( "message.text", messagebody );
	}
	catch( const pt::ptree_error &err )
	{
		// 其他错误，忽略.
	}

	try
	{
		broadcast_message( msg );
	}
	catch( const pt::ptree_error &err )
	{
		// 忽略.
		return avhttpd::errc::internal_server_error;
	}

	return 200;
}

void avbot_rpc_server::get_response_sended(boost::shared_ptr< boost::asio::streambuf > v,
	boost::system::error_code ec, std::size_t bytes_transfered)
{
	m_socket->get_io_service().post(
		boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), ec, bytes_transfered)
	);
}

// 发送数据在这里
void avbot_rpc_server::on_pop(boost::shared_ptr<boost::asio::streambuf> v)
{
	avhttpd::response_opts opts;
	opts.insert(avhttpd::http_options::content_type, "application/json; charset=utf8");
	opts.insert(avhttpd::http_options::content_length, boost::lexical_cast<std::string>(v->size()));
	opts.insert("Cache-Control", "no-cache");
	opts.insert(avhttpd::http_options::connection, "keep-alive");
	opts.insert(avhttpd::http_options::http_version,
		m_request.find(avhttpd::http_options::http_version)
	);

	avhttpd::async_write_response(
		*m_socket, 200, opts, *v,
		boost::bind<void>(&avbot_rpc_server::get_response_sended, shared_from_this(), v,  _1, _2)
	);
}

void avbot_rpc_server::done_search(boost::system::error_code ec, boost::property_tree::ptree jsonout)
{
	boost::shared_ptr<boost::asio::streambuf> v = boost::make_shared<boost::asio::streambuf>();
	std::ostream outstream(v.get());
	boost::property_tree::json_parser::write_json(outstream, jsonout);

	avhttpd::response_opts opts;
	opts.insert(avhttpd::http_options::content_type, "application/json; charset=utf8");
	opts.insert(avhttpd::http_options::content_length, boost::lexical_cast<std::string>(v->size()));
	opts.insert("Cache-Control", "no-cache");
	opts.insert(avhttpd::http_options::connection, "keep-alive");
	opts.insert(avhttpd::http_options::http_version,
		m_request.find(avhttpd::http_options::http_version)
	);


	avhttpd::async_write_response(
		*m_socket, 200, opts, *v,
		boost::bind<void>(&avbot_rpc_server::get_response_sended, shared_from_this(), v,  _1, _2)
	);
}

// 数据操作跑这里，嘻嘻.
void avbot_rpc_server::client_loop(boost::system::error_code ec, std::size_t bytestransfered)
{
	boost::smatch what;
	//for (;;)
	reenter(this)
	{for (;;){

		m_request.clear();
		m_streambuf = boost::make_shared<boost::asio::streambuf>();

		// 读取用户请求.
		yield avhttpd::async_read_request(
				*m_socket, *m_streambuf, m_request,
				boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), _1, 0)
		);

		if(ec)
		{
			if (ec == avhttpd::errc::post_without_content)
			{
				yield avhttpd::async_write_response(*m_socket, avhttpd::errc::no_content,
					boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), _1, 0)
				);
				return;
			}
			else if (ec == avhttpd::errc::header_missing_host)
			{
				yield avhttpd::async_write_response(*m_socket, avhttpd::errc::bad_request,
					boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), _1, 0)
				);
				return;
			}
			return;
		}

		// 解析 HTTP
		if(m_request.find(avhttpd::http_options::request_method) == "GET" )
		{
			if(m_request.find(avhttpd::http_options::request_uri)=="/message")
			{
				// 等待消息, 并发送.
				yield m_responses.async_pop(
					boost::bind(&avbot_rpc_server::on_pop, shared_from_this(), _2)
				);
			}
			else if(
				boost::regex_match(
					m_request.find(avhttpd::http_options::request_uri),
					what,
					boost::regex("/search\\?channel=([^&]*)&q=([^&]*)&date=([^&]*).*")
				)
			)
			{
				// 取出这几个参数, 到数据库里查找, 返回结果吧.
				yield do_search(what[1],what[2],what[3],
					boost::bind(&avbot_rpc_server::done_search, shared_from_this(), _1, _2)
				);
				return;
			}
			else if(
				boost::regex_match(
					m_request.find(avhttpd::http_options::request_uri),
					what,
					boost::regex("/search(\\?)?")
				)
			)
			{
				// missing parameter
				yield avhttpd::async_write_response(
					*m_socket,
					avhttpd::errc::internal_server_error,
					boost::bind(
						&avbot_rpc_server::client_loop,
						shared_from_this(),
						_1, 0
					)
				);
				return;
			}
			else
			{
				yield avhttpd::async_write_response(
					*m_socket,
					avhttpd::errc::not_found,
					boost::bind(
						&avbot_rpc_server::client_loop,
						shared_from_this(),
						_1, 0
					)
				);
				return;
			}
		}
		else if( m_request.find(avhttpd::http_options::request_method) == "POST")
		{
			// 这里进入 POST 处理.
			// 读取 body
			yield boost::asio::async_read(
				*m_socket,
				*m_streambuf,
				boost::asio::transfer_exactly(
					boost::lexical_cast<std::size_t>(
						m_request.find(avhttpd::http_options::content_length)
					) - m_streambuf->size()
				),
				boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), _1, _2 )
			);
			// body 必须是合法有效的 JSON 格式
			yield avhttpd::async_write_response(
					*m_socket,
					process_post(m_streambuf->size()),
					avhttpd::response_opts()
						(avhttpd::http_options::content_length, "4")
						(avhttpd::http_options::content_type, "text/plain")
						("Cache-Control", "no-cache")
						(avhttpd::http_options::http_version,
							m_request.find(avhttpd::http_options::http_version)),
					boost::asio::buffer("done"),
					boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), _1, 0)
			);
			if ( m_request.find(avhttpd::http_options::connection) != "keep-alive" )
				return;
		}

		// 继续
		yield avloop_idle_post(m_socket->get_io_service(),
			boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), ec, 0)
		);
	}}
}

void avbot_rpc_server::callback_message(const boost::property_tree::ptree& jsonmessage)
{
	boost::shared_ptr<boost::asio::streambuf> buf(new boost::asio::streambuf);
	std::ostream stream(buf.get());
	std::stringstream teststream;

	js::write_json(stream, jsonmessage);

	m_responses.push(buf);
}

}
