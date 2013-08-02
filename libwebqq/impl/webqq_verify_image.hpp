
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
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include "webqq_impl.hpp"

#include "constant.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

class get_verify_image_op {
public:
	get_verify_image_op(boost::shared_ptr<WebQQ> webqq, std::string vcimgid, webqq::webqq_handler_string_t handler)
	  : m_webqq(webqq), m_handler(handler),
		m_stream(boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()))),
		m_buffer(boost::make_shared<boost::asio::streambuf>())
	{
		BOOST_ASSERT(vcimgid.length() > 8);
		std::string url = boost::str(
							boost::format( LWQQ_URL_VERIFY_IMG ) % APPID % m_webqq->m_qqnum
						);

		m_stream->request_options(
			avhttp::request_opts()
			( avhttp::http_options::cookie, std::string( "chkuin=" ) + m_webqq->m_qqnum )
			( avhttp::http_options::connection, "close" )
		);
		avhttp::async_read_body( *m_stream, url, * m_buffer, *this);

	}
	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
		m_webqq->m_cookie_mgr.set_cookie(*m_stream);

		std::string vcimg;
		vcimg.resize(bytes_transfered);

		m_buffer->sgetn(&vcimg[0], bytes_transfered);

		if (ec)
			ec = error::fetch_verifycode_failed;
		m_handler(ec, vcimg);
	}
private:
	boost::shared_ptr<WebQQ> m_webqq;
	webqq::webqq_handler_string_t m_handler;

	boost::shared_ptr<boost::asio::streambuf> m_buffer;
	read_streamptr m_stream;

};


}
}
}
