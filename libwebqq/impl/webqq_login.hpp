
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
#include <boost/regex.hpp>

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include <boost/hash.hpp>

#include "boost/timedcall.hpp"
#include "boost/urlencode.hpp"
#include "boost/consolestr.hpp"

#include "webqq_impl.hpp"

#include "constant.hpp"

#include "webqq_status.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

inline std::string lutil_md5_data(const std::string & data)
{
	boost::hashes::md5::digest_type md5sum ;
	md5sum = boost::hashes::compute_digest<boost::hashes::md5>(data);
	return md5sum.str();
}

inline std::string lutil_md5_digest(const std::string & data)
{
	boost::hashes::md5::digest_type md5sum ;
	md5sum = boost::hashes::compute_digest<boost::hashes::md5>(data);
	return std::string(reinterpret_cast<const char*>(md5sum.c_array()), md5sum.static_size);
}

inline std::string uin_decode(const std::string &uin)
{
	int i;
	int uin_byte_length;
	char _uin[9] = {0};

	/* Calculate the length of uin (it must be 8?) */
	uin_byte_length = uin.length() / 4;

	/**
	 * Ok, parse uin from string format.
	 * "\x00\x00\x00\x00\x54\xb3\x3c\x53" -> {0,0,0,0,54,b3,3c,53}
	 */
	for( i = 0; i < uin_byte_length ; i++ ) {
		char u[5] = {0};
		char tmp;
		strncpy( u, & uin [  i * 4 + 2 ] , 2 );

		errno = 0;
		tmp = strtol( u, NULL, 16 );

		if( errno ) {
			return NULL;
		}

		_uin[i] = tmp;
	}
	return std::string(_uin, 8);
}

inline std::string generate_clientid()
{
	srand( time( NULL ) );
	return boost::str( boost::format( "%d%d%d" ) % (rand() % 90 + 10) % (rand() % 90 + 10) % (rand() % 90 + 10) );
}

// qq 登录办法-验证码登录
template<class Handler>
class SYMBOL_HIDDEN login_vc_op : boost::asio::coroutine{
public:
	login_vc_op(boost::shared_ptr<qqimpl::WebQQ> webqq, std::string _vccode, Handler handler)
		: m_webqq(webqq), vccode(_vccode), m_handler(handler)
	{
		std::string md5 = webqq_password_encode(m_webqq->m_passwd, vccode, m_webqq->m_verifycode.uin);

		// do login !
		std::string url = boost::str(
							  boost::format(
								  "%s/login?u=%s&p=%s&verifycode=%s&"
								  "webqq_type=%d&remember_uin=1&aid=%s&login2qq=1&"
								  "u1=%s&h=1&ptredirect=0&"
								  "ptlang=2052&from_ui=1&daid=164&pttype=1&dumy=&fp=loginerroralert&"
								  "action=4-15-8246&mibao_css=m_webqq&t=2&g=1&login_sig=%s")
							  % LWQQ_URL_LOGIN_HOST
							  % m_webqq->m_qqnum
							  % md5
							  % vccode
							  % m_webqq->m_status
							  % APPID
							  % boost::url_encode(std::string("http://web2.qq.com/loginproxy.html?login2qq=1&webqq_type=10"))
							  % m_webqq->m_login_sig
						  );

		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
		m_stream->request_options(
			avhttp::request_opts()
			(avhttp::http_options::cookie, m_webqq->m_cookie_mgr.get_cookie(url)())
			(avhttp::http_options::connection, "close")
		);

		m_buffer = boost::make_shared<boost::asio::streambuf>();

		avhttp::async_read_body(*m_stream, url, *m_buffer, *this);
	}

	// 在这里实现　QQ 的登录.
	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{
			if( ( check_login( ec, bytes_transfered ) == 0 ) && ( m_webqq->m_status == LWQQ_STATUS_ONLINE ) )
			{
				BOOST_LOG_TRIVIAL(info) <<  "redirecting to " << m_next_url;

				// 再次　login
				m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_webqq->get_ioservice()));
				m_stream->request_options(
					avhttp::request_opts()
					(avhttp::http_options::cookie, m_webqq->m_cookie_mgr.get_cookie(m_next_url)())
					(avhttp::http_options::connection, "close")
					(avhttp::http_options::referer, "https://ui.ptlogin2.qq.com/cgi-bin/login?daid=164&target=self&style=5&mibao_css=m_webqq&appid=1003903&enable_qlogin=0&no_verifyimg=1&s_url=http%3A%2F%2Fweb2.qq.com%2Floginproxy.html&f_url=loginerroralert&strong_login=1&login_state=10&t=20130723001")
					(avhttp::http_options::accept, "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
					(avhttp::http_options::user_agent, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/29.0.1547.32 Safari/537.36")
				);

				m_stream->max_redirects(0);

				m_buffer = boost::make_shared<boost::asio::streambuf>();

				BOOST_ASIO_CORO_YIELD avhttp::async_read_body(
					* m_stream, m_next_url, *m_buffer,	*this);

				m_webqq->m_cookie_mgr.save_cookie(*m_stream);

				m_buffer = boost::make_shared<boost::asio::streambuf>();

				if (ec == avhttp::errc::found){
					m_stream->request_options(
						avhttp::request_opts()
						(avhttp::http_options::cookie, m_webqq->m_cookie_mgr.get_cookie(m_stream->location())())
						(avhttp::http_options::connection, "close")
						(avhttp::http_options::accept, "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
						(avhttp::http_options::user_agent, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/29.0.1547.32 Safari/537.36")
					);
					BOOST_ASIO_CORO_YIELD avhttp::async_read_body(*m_stream, m_stream->location(), *m_buffer, *this);
					m_webqq->m_cookie_mgr.save_cookie(*m_stream);
				}

				BOOST_LOG_TRIVIAL(info) <<  "redirecting success!!";

				if (m_webqq->m_clientid.empty())
				{
					m_webqq->m_clientid = generate_clientid();
					m_webqq->m_cookie_mgr.save_cookie(
						"psession.qq.com", "/", "clientid", m_webqq->m_clientid, "session"
					);
				}
				//change status,  this is the last step for login
				// 设定在线状态.

				BOOST_LOG_TRIVIAL(info) <<  "changing status...";

				BOOST_ASIO_CORO_YIELD async_change_status(
					m_webqq, LWQQ_STATUS_ONLINE,
					boost::bind<void>(*this, _1, 0)
				);

				if(ec)
				{
					// 修改在线状态失败!
					m_webqq->get_ioservice().post(boost::asio::detail::bind_handler(m_handler, ec));
					return;
				}

				BOOST_LOG_TRIVIAL(info) <<  "status => online";

				i = 0;

				//polling group list

				// 重试五次，每次延时，如果还失败， 只能说登录失败.
				do{
					BOOST_ASIO_CORO_YIELD m_webqq->update_group_list(boost::bind<void>(*this, _1, 0));
					if ( ec )
					{
						BOOST_LOG_TRIVIAL(warning) << "刷新群列表失败，第 " <<  i << " 次重试中(共五次)..." ;

						BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
							m_webqq->get_ioservice(), i*20+50,
							boost::asio::detail::bind_handler(*this, ec, 0)
						);

					}
				}while (i++ < 5 && ec);
				if (ec){
					// 刷群列表失败，登录也就失败了.
					return m_webqq->get_ioservice().post(
						boost::asio::detail::bind_handler(m_handler, ec)
					);
				}

				// 接着是刷新群成员列表.
				for (iter = m_webqq->m_groups.begin(); iter != m_webqq->m_groups.end(); ++iter)
				{
					BOOST_ASIO_CORO_YIELD
						m_webqq->update_group_member(iter->second , boost::bind<void>(*this, _1, 0));

					BOOST_ASIO_CORO_YIELD
						boost::delayedcallms(m_webqq->get_ioservice(), 530, boost::bind<void>(*this, ec, 0));
				}
			}
			return m_webqq->get_ioservice().post(
				boost::asio::detail::bind_handler(m_handler, ec)
			);
		}
	}

private:

	/**
	* I hacked the javascript file named comm.js, which received from tencent
	* server, and find that fuck tencent has changed encryption algorithm
	* for password in webqq3 . The new algorithm is below(descripted with javascript):
	* var M=C.p.value; // M is the qq password
	* var I=hexchar2bin(md5(M)); // Make a md5 digest
	* var H=md5(I+pt.uin); // Make md5 with I and uin(see below)
	* var G=md5(H+C.verifycode.value.toUpperCase());
	*
	* @param pwd User's password
	* @param vc Verify Code. e.g. "!M6C"
	* @param uin A string like "\x00\x00\x00\x00\x54\xb3\x3c\x53", NB: it
	*        must contain 8 hexadecimal number, in this example, it equaled
	*        to "0x0,0x0,0x0,0x0,0x54,0xb3,0x3c,0x53"
	*
	* @return Encoded password
	*/
	std::string webqq_password_encode( const std::string & pwd, const std::string & vc, const std::string & uin)
	{
		/* Equal to "var I=hexchar2bin(md5(M));" */
		std::string I =  lutil_md5_digest(pwd);

		/* Equal to "var H=md5(I+pt.uin);" */
		std::string H = lutil_md5_data( I + uin_decode(uin));

		/* Equal to var G=md5(H+C.verifycode.value.toUpperCase()); */
		std::string G =  lutil_md5_data(boost::to_upper_copy(H + vc));
		return boost::to_upper_copy(G);
	}

private:
	int check_login(boost::system::error_code & ec, std::size_t bytes_transfered)
	{
		std::string response;
		response.resize(bytes_transfered);
		m_buffer->sgetn(&response[0], bytes_transfered);

		BOOST_LOG_TRIVIAL(debug) << console_out_str(response);

		int status;
		boost::cmatch what;
		boost::regex ex("ptuiCB\\('([0-9])',[ ]?'([0-9])',[ ]?'([^']*)',[ ]?'([0-9])',[ ]?'([^']*)',[ ]?'([^']*)'[ ]*\\);");

		if(boost::regex_search(response.c_str(), what, ex))
		{
			status = boost::lexical_cast<int>(what[1]);
			m_webqq->m_nick = what[6];
			m_next_url = what[3];
		}else
			status = 9;

		if ( (status >= 0 && status <= 8) || status == error::login_failed_blocked_account)
		{
			ec = error::make_error_code(static_cast<error::errc_t>(status));
		}else{
			ec = error::login_failed_other;
		}

		if (!ec){
			m_webqq->m_status = LWQQ_STATUS_ONLINE;
			m_webqq->m_cookie_mgr.save_cookie(*m_stream);
			BOOST_LOG_TRIVIAL(info) <<  "login success!";
		}else{
			status = LWQQ_STATUS_OFFLINE;
			BOOST_LOG_TRIVIAL(info) <<  "login failed!!!!  " <<  ec.message();
		}

		return status;
	}

private:
	boost::shared_ptr<qqimpl::WebQQ> m_webqq;
	Handler m_handler;
	std::string m_next_url;

	read_streamptr m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffer;
	std::string vccode;

private:
	int i;
	grouplist::iterator iter;
};

template<class Handler>
login_vc_op<Handler> make_async_login_vc_op(boost::shared_ptr<qqimpl::WebQQ> webqq, std::string _vccode, Handler handler)
{
	return login_vc_op<Handler>(webqq, _vccode, handler);
}

} // namespace detail


template<class Handler>
void async_login(boost::shared_ptr<qqimpl::WebQQ> webqq, std::string _vccode, Handler handler)
{
	detail::make_async_login_vc_op(webqq, _vccode, handler);
}

} // namespace qqimpl
} // namespace webqq
