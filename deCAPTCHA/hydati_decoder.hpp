/*
 * Copyright (C) 2013  微蔡 <microcai@fedoraproject.org>
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
#include <string>
#include <fstream>
#include <boost/regex.hpp>
#include <boost/random.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include <boost/property_tree/ptree.hpp>
namespace pt = boost::property_tree;
#include <boost/property_tree/json_parser.hpp>
namespace js = boost::property_tree::json_parser;

#include <boost/timedcall.hpp>

#ifndef BOOST_SYSTEM_NOEXCEPT
  #define BOOST_SYSTEM_NOEXCEPT BOOST_NOEXCEPT
#endif

namespace decaptcha{
namespace decoder{
namespace hydati{
namespace detail {
	class error_category_impl;
}

template<class error_category>
const boost::system::error_category& error_category_single()
{
	static error_category error_category_instance;
	return reinterpret_cast<const boost::system::error_category&>(error_category_instance);
}

inline const boost::system::error_category& error_category()
{
	return error_category_single<detail::error_category_impl>();
}

namespace error{
enum errc_t{
	ERROR_CAPCHA_NOT_READY = 1,
	ERROR_WRONG_USER_KEY,
	ERROR_KEY_DOES_NOT_EXIST,
	ERROR_NO_SLOT_AVAILABLE,
	ERROR_ZERO_CAPTCHA_FILESIZE,
	ERROR_TOO_BIG_CAPTCHA_FILESIZE,
	ERROR_ZERO_BALANCE,
	ERROR_IP_NOT_ALLOWED,
	ERROR_CAPTCHA_UNSOLVABLE,
	ERROR_BAD_DUPLICATES,
	ERROR_NO_SUCH_METHOD,
	ERROR_IMAGE_TYPE_NOT_SUPPORTED,
	ERROR_INTERNAL_SERVER_ERROR,
};

inline boost::system::error_code make_error_code(errc_t e)
{
	return boost::system::error_code(static_cast<int>(e), error_category());
}

} // namespace error
} // namespace hydati
} // namespace decoder
} // namespace decaptcha

namespace boost {
namespace system {

template <>
struct is_error_code_enum<decaptcha::decoder::hydati::error::errc_t>
{
  static const bool value = true;
};

} // namespace system
} // namespace boost


namespace decaptcha{
namespace decoder{
namespace hydati{
namespace detail{

class error_category_impl
  : public boost::system::error_category
{
	virtual const char* name() const BOOST_SYSTEM_NOEXCEPT
	{
		return "hydati";
	}

	virtual std::string message(int e) const
	{
		switch (e)
		{
		case error::ERROR_CAPCHA_NOT_READY:
			return "captcha is not recognized yet, repeat request withing 1-5 seconds";
		case error::ERROR_WRONG_USER_KEY:
			return "user authorization key is invalid (its length is not 32 bytes as it should be)";
		case error::ERROR_KEY_DOES_NOT_EXIST:
			return "you have set wrong user authorization key in request";
		case error::ERROR_NO_SLOT_AVAILABLE:
			return "no idle captcha workers are available at the moment, please try a bit later or try increasing your bid";
		case error::ERROR_ZERO_CAPTCHA_FILESIZE:
			return "the size of the captcha you are uploading is zero";
		case error::ERROR_TOO_BIG_CAPTCHA_FILESIZE:
			return "your captcha size is exceeding 100kb limit";
		case error::ERROR_ZERO_BALANCE:
			return "account has zero or negative balance";
		case error::ERROR_IP_NOT_ALLOWED:
			return "Request with current account key is not allowed from your IP. Please refer to IP list section";
		case error::ERROR_CAPTCHA_UNSOLVABLE:
			return "Could not solve captcha in 6 attempts by different workers";
		case error::ERROR_BAD_DUPLICATES:
			return "100% recognition feature failed due to attempts limit";
		case error::ERROR_NO_SUCH_METHOD:
			return "You must send method parameter in your API request, please refer to the API documentation";
		case error::ERROR_IMAGE_TYPE_NOT_SUPPORTED:
			return "Could not determine captcha file type, only allowed formats are JPG, GIF, PNG";
		case error::ERROR_INTERNAL_SERVER_ERROR:
			return "internal server error";
		default:
			return "hydati ERROR";
		}
	}
};

struct report_bad_op : boost::asio::coroutine
{
	report_bad_op(boost::asio::io_service & io_service, const std::string &authkey,
			boost::shared_ptr<std::string> CAPTCHA_ID)
	  : m_io_service(io_service), m_CAPTCHA_ID(CAPTCHA_ID), m_authkey(authkey)
	{
	}

	// 调用这个开始报告错误.
	void operator()()
	{
		// http://dt1.hydati.com:8080/response.php?action=error&auth_code=xxxx&sid=xxxxxx
		m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_io_service));
		m_buffers = boost::make_shared<boost::asio::streambuf>();

		avhttp::async_read_body(*m_stream,
								std::string(boost::str(boost::format("http://dt1.hydati.com:8080/response.php?action=error&auth_code=%s&sid=%s") % m_authkey % *m_CAPTCHA_ID)),
								*m_buffers, *this);
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{

	}

private:
	boost::asio::io_service & m_io_service;
	const std::string m_authkey;
	boost::shared_ptr<std::string> m_CAPTCHA_ID;

	boost::shared_ptr<avhttp::http_stream> m_stream;
	boost::shared_ptr<boost::asio::streambuf> m_buffers;
};

inline report_bad_op report_bad_func(boost::asio::io_service & io_service,
				const std::string & authkey,
				boost::shared_ptr<std::string> CAPTCHA_ID)
{
	return report_bad_op(io_service, authkey, CAPTCHA_ID);
}

template<class Handler>
class hydati_decoder_op : boost::asio::coroutine
{
	std::string generate_boundary() const
	{
		boost::rand48 p(time(NULL));
		return  boost::str(boost::format("%06x%06x") % p() %  p()  ).substr(0, 12);
	}
public:
	hydati_decoder_op(boost::asio::io_service & io_service,
			const std::string &authkey,const std::string &dati_type,
			const std::string &buffer, Handler handler)
		: m_io_service(io_service),
		  m_authkey(authkey),
		  m_dati_type(dati_type),
		  m_handler(handler),
		  m_stream(boost::make_shared<avhttp::http_stream>(boost::ref(m_io_service))),
		  m_CAPTCHA_ID(boost::make_shared<std::string>()),
		  m_buffers(boost::make_shared<boost::asio::streambuf>()),
		  m_tries(0), stop_tries(false)
	{
		std::string boundary = generate_boundary();
 		std::string content = build_multipart_formdata(buffer, boundary);
 		std::string content_type = boost::str(boost::format("multipart/form-data; boundary=----------------------------%s") % boundary);

		m_stream->request_options(
			avhttp::request_opts()
				(avhttp::http_options::request_method, "POST" )
				(avhttp::http_options::connection, "close" )
				(avhttp::http_options::content_length, boost::lexical_cast<std::string>(content.length()) )
				(avhttp::http_options::content_type, content_type )
				(avhttp::http_options::request_body, content )
		);
		// 处理.
		avhttp::async_read_body(*m_stream, "http://dt1.hydati.com:8080/uploadpic.php", *m_buffers, *this);
	};

	// 这里是返回的数据
	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
		using namespace boost::system::errc;

 		BOOST_ASIO_CORO_REENTER(this)
 		{
			if (!process_upload_result(ec, bytes_transfered))
			{
				m_io_service.post(
					boost::asio::detail::bind_handler(
						m_handler, ec, std::string("慧眼答题平台"), std::string(""), boost::function<void()>()
					)
				);

				return;
			}

			// 延时 3s
			BOOST_ASIO_CORO_YIELD
					boost::delayedcallsec(m_io_service, 3, boost::asio::detail::bind_handler(*this, ec, 0) );

			do{
				// 延时 2s 然后重试.
				// 这样第一次就延时 5s 了,  正好合适
				BOOST_ASIO_CORO_YIELD
					boost::delayedcallsec(m_io_service, 2, boost::asio::detail::bind_handler(*this, ec, 0) );

				// 获取一下结果
				m_stream = boost::make_shared<avhttp::http_stream>(boost::ref(m_io_service));
				m_buffers = boost::make_shared<boost::asio::streambuf>();

				// http://www.hydati.com/index.php?mod=demo&act=result&id=CAPCHA_ID_HERE
				BOOST_ASIO_CORO_YIELD
					avhttp::async_read_body(*m_stream,
											std::string(boost::str(boost::format("http://dt1.hydati.com:8080/query.php?sid=%s") % *m_CAPTCHA_ID)),
											*m_buffers, *this);

				if (process_result(ec, bytes_transfered))
					return;
			}while (should_try(ec));

			m_io_service.post(
				boost::asio::detail::bind_handler(
					m_handler, make_error_code(operation_canceled), std::string("慧眼答题平台"), std::string(""), boost::function<void()>()
				)
			);
 		}
	}
private:
	bool process_result(boost::system::error_code & ec, std::size_t bytes_transfered)
	{
		// 如果返回为空字符串，那么表示题目还在答题，否则是答案
		// 如果出错返回‘#’开头的错误信息字符串。例如:#答题超时
 		using namespace boost::system::errc;

		std::string response;

		response.resize(bytes_transfered);

		m_buffers->sgetn(&response[0],bytes_transfered);

		if(response.empty())
		{
			return false;
		}

		if(boost::regex_match(response,boost::regex("([a-zA-Z]+)")))
		{
			using namespace boost::asio::detail;

			m_io_service.post(
					bind_handler(
						m_handler,
						boost::system::error_code(),
						std::string("慧眼答题平台"),
						response,
						report_bad_func(m_io_service, m_authkey, m_CAPTCHA_ID)
					)
				);
			return true;
		}

		ec = error::ERROR_CAPTCHA_UNSOLVABLE;
		if(response[0]=='#')
			stop_tries = true;
		return false;
	}

	/*
	 * 返回的数据是ID或者 #开头的错误描述.
	 *
     */
	bool process_upload_result(boost::system::error_code & ec, std::size_t bytes_transfered)
	{
		// 检查result
		std::string result;
		result.resize(bytes_transfered);
		m_buffers->sgetn(&result[0], bytes_transfered);

 		boost::cmatch what;
 		boost::regex ex("([\\d]*)");
		if(result[0] != '#' && boost::regex_search(result.c_str(), what, ex))
		{
			// 获得了 ID
			* m_CAPTCHA_ID = what[1];
			return true;
		};

		ec = error::ERROR_CAPTCHA_UNSOLVABLE;
		stop_tries = true;
		return false;
	}

	// 神码叫应该继续呢?  就是返回没错误, 也没有超过 1min
	bool should_try(boost::system::error_code ec)
	{
		return stop_tries==false && (m_tries ++ < 5);
	}

private:
	std::string build_multipart_formdata(const std::string &buffer, const std::string & boundary) const
	{
		std::stringstream content_body;


		content_body << "------------------------------" << boundary <<  "\r\n"
					 << "Content-Disposition: form-data; name=\"dati_type\"" <<  "\r\n\r\n"
					 << m_dati_type << "\r\n";
		content_body << "------------------------------" <<  boundary <<  "\r\n"
					 << "Content-Disposition: form-data; name=\"acc_str\"" <<  "\r\n\r\n"
					 << m_authkey << "\r\n";

		content_body << "------------------------------" <<  boundary <<  "\r\n"
					 << "Content-Disposition: form-data; name=\"timeout\"" <<  "\r\n\r\n"
					 << "40" << "\r\n";

		content_body << "------------------------------" <<  boundary <<  "\r\n"
					 << "Content-Disposition: form-data; name=\"pic\"; filename=\"vercode.jpeg\"" << "\r\n"
					 << "Content-Type: image/jpeg" <<  "\r\n\r\n";

		content_body.write(buffer.data(), buffer.length());
			content_body << "\r\n";

		content_body << "------------------------------" <<  boundary <<  "\r\n"
					 << "Content-Disposition: form-data; name=\"extra_str\"" <<  "\r\n\r\n"
					 << "四个字母 不区分大小写" << "\r\n";
		content_body << "------------------------------" <<  boundary << "--" << "\r\n";
		return content_body.str();
	}

private:
	boost::asio::io_service & m_io_service;

	int m_tries;
	bool stop_tries;

	boost::shared_ptr<avhttp::http_stream> m_stream;

	boost::shared_ptr<boost::asio::streambuf> m_buffers;

	boost::shared_ptr<std::string> m_CAPTCHA_ID;

	Handler m_handler;

	const std::string m_authkey;
	const std::string m_dati_type;
};

} // namespace detail
} // namespace hydati

class hydati_decoder{
public:
	hydati_decoder(boost::asio::io_service & io_service, const std::string &authkey)
	  : m_io_service(io_service), m_authkey(authkey)
	{
	}

	template <class Handler>
	void operator()(const std::string &buffer, Handler handler)
	{
		hydati::detail::hydati_decoder_op<Handler>
				op(m_io_service, m_authkey, "1000",buffer, handler);
	}
private:
	boost::asio::io_service & m_io_service;
	std::string m_authkey;
};

}
}

