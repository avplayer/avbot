/*
 * <one line to give the program's name and a brief idea of what it does.>
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

#include <string>
#include <vector>
#include <map>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/signals2.hpp>
#include <boost/any.hpp>

#if defined _WIN32 || defined __CYGWIN__
#ifdef BUILDING_DLL
#ifdef __GNUC__
#define DLL_PUBLIC __attribute__ ((dllexport))
#else
#define DLL_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define DLL_PUBLIC __attribute__ ((dllimport))
#else
#define DLL_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif
#define DLL_LOCAL
#else
#if __GNUC__ >= 4
#define DLL_PUBLIC __attribute__ ((visibility ("default")))
#define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#else
#define DLL_PUBLIC
#define DLL_LOCAL
#endif
#endif

namespace webqq{
namespace qqimpl {
class WebQQ;
};

typedef enum LWQQ_STATUS {
	LWQQ_STATUS_UNKNOW = 0,
	LWQQ_STATUS_ONLINE = 10,
	LWQQ_STATUS_OFFLINE = 20,
	LWQQ_STATUS_AWAY = 30,
	LWQQ_STATUS_HIDDEN = 40,
	LWQQ_STATUS_BUSY = 50,
	LWQQ_STATUS_CALLME = 60,
	LWQQ_STATUS_SLIENT = 70,
	LWQQ_STATUS_QUITTING = 1000
} LWQQ_STATUS;

struct qqBuddy {
	// 号码，每次登录都变化的.
	const std::string uin;

	// qq昵称.
	const std::string nick;
	// 群昵称.
	const std::string card;

	// 成员类型. 21/20/85 是管理员.
	const unsigned int mflag;

	// qq号码，不一定有，需要调用 get_qqnumber后才有.
	const std::string qqnum;

	qqBuddy(std::string _uin, std::string _nick, std::string _card,
		unsigned int _mflag,std::string _qqnum)
		: uin(_uin)
		, nick(_nick)
		, card(_card)
		, mflag(_mflag)
		, qqnum(_qqnum)
	{
	}
};

typedef boost::shared_ptr<qqBuddy> qqBuddy_ptr;

// 群.
struct qqGroup {
	// 群ID, 不是群的QQ号，每次登录都变化的.
	std::string gid;
	// 群名字.
	std::string name;

	// 群代码，可以用来获得群QQ号.
	std::string code;
	// 群QQ号.
	std::string qqnum;

	std::string owner;

	boost::function<
		qqBuddy_ptr (std::string uin)
	> get_Buddy_by_uin;
	boost::function<
		void (std::string uin, std::string qqnum, std::string nick)
	> add_new_buddy;
};

typedef boost::shared_ptr<qqGroup> qqGroup_ptr;

struct qqMsgCface{
	std::string gid, uin, vfwebqq;
	std::string name, file_id, key, server;
	std::string cookie;
	std::string gchatpicurl;
};

struct qqMsg {
	enum {
		LWQQ_MSG_FONT,
		LWQQ_MSG_TEXT,
		LWQQ_MSG_FACE,
		LWQQ_MSG_CFACE,
	} type;
	std::string font;//font name, size color.
	std::string text;
	int face;
	struct qqMsgCface cface;
	std::string	cface_data;
};

class webqq {
public:
	typedef boost::function<void(boost::system::error_code)> webqq_handler_t;
	typedef boost::function<void(boost::system::error_code, std::string )> webqq_handler_string_t;
 	typedef boost::function<void(boost::system::error_code, int ,  boost::any )> webqq_handler_int_any_t;
// 	typedef boost::function<void(boost::system::error_code )> webqq_handler_t;
// 	typedef boost::function<void(boost::system::error_code )> webqq_handler_t;

public:
	webqq(boost::asio::io_service & asioservice, std::string qqnum, std::string passwd, bool no_persistent_db=false);
	~webqq();
	// 设置受到群消息的回调.
	void on_group_msg( boost::function<void ( const std::string group_code, const std::string who, const std::vector<qqMsg> & )> cb );
	// 发现一个群就回调.
	void on_group_found(boost::function<void ( qqGroup_ptr )> cb);
	// 新人入群通知. 注意, 只有管理员才能获得.
	void on_group_newbee(boost::function<void (qqGroup_ptr, qqBuddy_ptr)> cb);

	bool is_online();

	void on_verify_code( boost::function<void ( std::string )> );
	// login with vc, call this if you got signeedvc signal.
	// in signeedvc signal, you can retreve images from server.
	void feed_vc( std::string vccode, boost::function<void()> bad_vcreporter);

	void send_group_message( std::string group, std::string msg, boost::function<void ( const boost::system::error_code& ec )> donecb );
	void send_group_message( qqGroup &  group, std::string msg, boost::function<void ( const boost::system::error_code& ec )> donecb );

	void update_group_member(boost::shared_ptr<qqGroup> group );

	static void async_fetch_cface(boost::asio::io_service & , const qqMsgCface & cface, boost::function<void(boost::system::error_code ec, boost::asio::streambuf & buf)> callback);
	static void async_fetch_cface_std_saver(boost::system::error_code ec, boost::asio::streambuf & buf, std::string cface, boost::filesystem::path parent_path);

	// get final url of the cface image
	static void async_cface_url_final(boost::asio::io_service & , const qqMsgCface & cface, boost::function<void(boost::system::error_code ec, std::string)> callback);

	typedef boost::function<void(qqGroup_ptr group, bool needvc, const std::string & vc_img_data)>	search_group_handler;
	// 查找群，如果要验证码，则获取后带vfcode参数进行调用.否则对  vfcode 是 ""
	void search_group(std::string groupqqnum, std::string vfcode, search_group_handler handler);

	typedef boost::function<void(qqGroup_ptr group, bool needvc, const std::string & vc_img_data)>	join_group_handler;
	// 加入群，如果要验证码，则获取后带vfcode参数进行调用.否则对  vfcode 是 ""
	// group 是 search_group 返回的那个.
	void join_group(qqGroup_ptr group, std::string vfcode, webqq::join_group_handler handler);

	qqGroup_ptr get_Group_by_gid( std::string );
	qqGroup_ptr get_Group_by_qq( std::string qq );
	boost::asio::io_service	&get_ioservice();
private:
	boost::shared_ptr<qqimpl::WebQQ> impl;
};

}
