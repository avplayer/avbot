//
// error_code.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2009 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
// Copyright (c) 2013 microcai ( microcai at avplayer dot org )
//
//

#pragma once

#include <string>
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>

#ifndef BOOST_SYSTEM_NOEXCEPT
  #define BOOST_SYSTEM_NOEXCEPT BOOST_NOEXCEPT
#endif

namespace webqq {
namespace error {

class error_category_impl;

const boost::system::error_category& error_category();


/// HTTP error codes.
/**
 * The enumerators of type @c errc_t are implicitly convertible to objects of
 * type @c boost::system::error_code.
 *
 * @par Requirements
 * @e Header: @c <error_codec.hpp> @n
 * @e Namespace: @c avhttp::errc
 */
enum errc_t
{
	ok = 0,

	login_failed_server_busy = 1,

	login_failed_qq_outdate,

	login_failed_wrong_passwd,

	login_failed_wrong_vc,

	login_failed_verify_failed,

	login_failed_try_again,

	login_failed_wrong_input,

	login_failed_too_many_login,

	login_failed_other,

	login_failed_blocked_account = 19,



	login_check_need_vc = 1000,

	fetch_verifycode_failed,

	failed_to_change_status,

	// 登录后的一些错误消息.

	failed_to_fetch_group_list,

	failed_to_fetch_group_qqnumber,

	// 工作起来后的一些错误消息.

	poll_failed_user_kicked_off, // 这个错误需要重新登录, 不过是要等一段时间.

	poll_failed_network_error, // 这个错误只需重试就可以了.

	poll_failed_need_login, // 对于这个错误,  重新登录就可以了.

	poll_failed_need_refresh, // 对于这个错误,  重新刷新群信息.

	poll_failed_user_quit, // 对于这个错误,  重新登录就可以了.

	poll_failed_unknow_ret_code, // 对于这个错误, 忽略？

	send_message_failed_not_login, // 未登录

	send_message_failed_too_long, // 对消息进行拆分

	send_message_failed_too_often, // 等几秒再发
};

/// Converts a value of type @c errc_t to a corresponding object of type
/// @c boost::system::error_code.
/**
 * @par Requirements
 * @e Header: @c <error_codec.hpp> @n
 * @e Namespace: @c avhttp::errc
 */
inline boost::system::error_code make_error_code(errc_t e)
{
	return boost::system::error_code(static_cast<int>(e), error_category());
}

class error_category_impl
  : public boost::system::error_category
{
	virtual const char* name() const BOOST_SYSTEM_NOEXCEPT;

	virtual std::string message(int e) const;
};

} // namespace error
} // namespace webqq


namespace boost {
namespace system {

template <>
struct is_error_code_enum<webqq::error::errc_t>
{
  static const bool value = true;
};

} // namespace system
} // namespace boost

