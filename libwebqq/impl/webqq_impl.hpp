/*
 * Copyright (C) 2012  微蔡 <microcai@fedoraproject.org>
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

#if defined _WIN32 || defined __CYGWIN__
#define SYMBOL_HIDDEN
#else
#if __GNUC__ >= 4
#define SYMBOL_HIDDEN  __attribute__ ((visibility ("hidden")))
#else
#define SYMBOL_HIDDEN
#endif
#endif


#include <string>
#include <map>
#include <queue>
#include <boost/tuple/tuple.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <boost/concept_check.hpp>
#include <boost/system/system_error.hpp>
#include <boost/property_tree/ptree.hpp>
namespace pt = boost::property_tree;
#include <boost/enable_shared_from_this.hpp>

#include <boost/async_coro_queue.hpp>

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>
typedef boost::shared_ptr<avhttp::http_stream> read_streamptr;

#include "../webqq.hpp"
#include "../error_code.hpp"
#include "cookie_mgr.hpp"
#include "buddy_mgr.hpp"

#if !defined(_MSC_VER)
#pragma GCC visibility push(hidden)
#endif

namespace webqq{
namespace qqimpl{

typedef enum LwqqMsgType {
	LWQQ_MT_BUDDY_MSG = 0,
	LWQQ_MT_GROUP_MSG,
	LWQQ_MT_DISCU_MSG,
	LWQQ_MT_SESS_MSG, //group whisper message
	LWQQ_MT_STATUS_CHANGE,
	LWQQ_MT_KICK_MESSAGE,
	LWQQ_MT_SYSTEM,
	LWQQ_MT_BLIST_CHANGE,
	LWQQ_MT_SYS_G_MSG,
	LWQQ_MT_OFFFILE,
	LWQQ_MT_FILETRANS,
	LWQQ_MT_FILE_MSG,
	LWQQ_MT_NOTIFY_OFFFILE,
	LWQQ_MT_INPUT_NOTIFY,
	LWQQ_MT_UNKNOWN,
} LwqqMsgType;

typedef enum {
	LWQQ_MC_OK = 0,
	LWQQ_MC_TOO_FAST = 108,             //< send message too fast
	LWQQ_MC_LOST_CONN = 121
} LwqqMsgRetcode;

typedef struct LwqqVerifyCode {
	std::string img;
	std::string uin;
	std::string data;
	size_t size;
} LwqqVerifyCode;

typedef std::map<std::string, boost::shared_ptr<qqGroup> > grouplist;

typedef boost::system::error_code system_error;

class SYMBOL_HIDDEN WebQQ  : public boost::enable_shared_from_this<WebQQ>
{
public:
	using boost::enable_shared_from_this<WebQQ>::shared_from_this;
public:
	WebQQ(boost::asio::io_service & asioservice, std::string qqnum, std::string passwd, bool no_persistent_db=false);

	// called by webqq.hpp
	void start();
	// called by webqq.hpp distructor
	void stop();
	
	// change status. This is the last step of login process.
	void change_status(LWQQ_STATUS status, boost::function<void (boost::system::error_code) > handler);

	void async_poll_message(webqq::webqq_handler_t handler);

	typedef boost::function<void ( const boost::system::error_code& ec )> send_group_message_cb;
	void send_group_message( std::string group, std::string msg, send_group_message_cb donecb );
	void send_group_message( qqGroup &  group, std::string msg, send_group_message_cb donecb );
	void update_group_list(webqq::webqq_handler_t handler);
	void update_group_qqnumber(boost::shared_ptr<qqGroup> group, webqq::webqq_handler_t handler);
	void update_group_member(boost::shared_ptr<qqGroup> group, webqq::webqq_handler_t handler);

	// 查找群，如果要验证码，则获取后带vfcode参数进行调用.否则对  vfcode 是 ""
	void search_group(std::string groupqqnum, std::string vfcode, webqq::search_group_handler handler);

	// 加入群，如果要验证码，则获取后带vfcode参数进行调用.否则对  vfcode 是 ""
	// group 是 search_group 返回的那个.
	void join_group(qqGroup_ptr group, std::string vfcode, webqq::join_group_handler handler);

	qqGroup_ptr get_Group_by_gid( std::string gid );
	qqGroup_ptr get_Group_by_qq( std::string qq );
	boost::asio::io_service	&get_ioservice() {
		return m_io_service;
	};

public:// signals
	// 验证码, 需要自行下载url中的图片，然后调用 login_withvc.
	boost::signals2::signal< void (std::string)> m_signeedvc;

	// 报告验证码不对
	boost::function<void ()> m_sigbadvc;

	// 获得一个群QQ号码的时候激发.
	boost::signals2::signal< void ( qqGroup_ptr )> siggroupnumber;
	// 新人入群的时候激发.
	boost::signals2::signal< void ( qqGroup_ptr, qqBuddy_ptr buddy) > signewbuddy;

	// 有群消息的时候激发.
	boost::signals2::signal< void ( const std::string group, const std::string who, const std::vector<qqMsg> & )> siggroupmessage;

public:
	void init_face_map();

	void update_group_member_qq(boost::shared_ptr<qqGroup> group );

	void get_verify_image( std::string vcimgid, webqq::webqq_handler_string_t handler);

	void cb_newbee_group_join(qqGroup_ptr group, std::string uid);

	void cb_search_group(std::string, const boost::system::error_code& ec, read_streamptr stream,  boost::shared_ptr<boost::asio::streambuf> buf, webqq::search_group_handler handler);
	
	void fetch_aid(std::string arg, boost::function<void(const boost::system::error_code&, std::string)> handler);
	void cb_fetch_aid(const boost::system::error_code& ec, read_streamptr stream,  boost::shared_ptr<boost::asio::streambuf> buf, boost::function<void(const boost::system::error_code&, std::string)> handler);

	void cb_join_group(qqGroup_ptr, const boost::system::error_code& ec, read_streamptr stream,  boost::shared_ptr<boost::asio::streambuf> buf, webqq::join_group_handler handler);

public:


public:
	boost::asio::io_service & m_io_service;

	std::string m_qqnum, m_passwd;
	LWQQ_STATUS m_status;

	std::string m_nick; // nick of the avbot account

	std::string	m_version;
	std::string m_clientid;
	std::string m_psessionid;
	std::string	m_vfwebqq;
	std::string m_login_sig; // 安全参数
	long m_msg_id; // update on every message.

	LwqqVerifyCode m_verifycode;

	grouplist	m_groups;

	std::map<int, int> facemap;

	// 消息发送异步列队
	boost::async_coro_queue<
		boost::circular_buffer<
			boost::tuple<std::string, std::string, send_group_message_cb>
		>
	> m_group_message_queue;

	// 用于获取验证码的异步列队
	boost::async_coro_queue<
		boost::circular_buffer<std::string>
	> m_vc_queue;

	typedef boost::tuple<
		webqq::webqq_handler_t  // 更新回调，如果push的那个数据被更新到了就会回调.
		, std::string   // 群 QQ 号  为空表示更新所有的群.
	> group_refresh_queue_type;

	// 用于重新刷新群列表的异步命令列队
	boost::async_coro_queue<
		// 列队里数据解释
		std::list<group_refresh_queue_type>
	> m_group_refresh_queue;

	cookie::cookie_store m_cookie_mgr;

	buddy_mgr	m_buddy_mgr;
};

};

} // namespace webqq

#if !defined(_MSC_VER)
#pragma GCC visibility pop
#endif
