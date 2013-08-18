#include <boost/regex.hpp>
#include <boost/throw_exception.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <fstream>

#include "boost/urlencode.hpp"
#include "avbot.hpp"


static std::string	preamble_formater(std::string preamble_qq_fmt, webqq::qqBuddy *buddy, std::string falbacknick, webqq::qqGroup * grpup = NULL )
{
	static webqq::qqBuddy _buddy("", "", "", 0, "");
	std::string preamble;
	// 格式化神器, 哦耶.
	// 获取格式化描述字符串.
	std::string preamblefmt = preamble_qq_fmt;
	// 支持的格式化类型有 %u UID,  %q QQ号, %n 昵称,  %c 群名片 %a 自动.

	preamble = preamblefmt;
	std::string autonick = "";

	if( !buddy ) {
		autonick = falbacknick;
		buddy = & _buddy;
	} else {
		autonick = buddy->card;

		if( autonick.empty() ) {
			autonick = buddy->nick;
		}

		if( autonick.empty() ) {
			autonick = buddy->qqnum;
		}

		if( autonick.empty() ) {
			autonick = buddy->uin;
		}
	}

	boost::replace_all( preamble, "%a", autonick );
	boost::replace_all( preamble, "%n", buddy->nick );
	boost::replace_all( preamble, "%u", buddy->uin );
	boost::replace_all( preamble, "%q", buddy->qqnum );
	boost::replace_all( preamble, "%c", buddy->card );

	if( grpup ){
		boost::replace_all( preamble, "%r", grpup->qqnum );
		boost::replace_all( preamble, "%R", grpup->name );
	}
	return preamble;
}

static std::string	preamble_formater(std::string preamble_irc_fmt, irc::irc_msg pmsg )
{
	// 格式化神器, 哦耶.
	// 获取格式化描述字符串.
	std::string preamble = preamble_irc_fmt;

	// 支持的格式化类型有 %u UID,  %q QQ号, %n 昵称,  %c 群名片 %a 自动 %r irc 房间.
	// 默认为 qq(%a) 说:.
	boost::replace_all( preamble, "%a", pmsg.whom );
	boost::replace_all( preamble, "%r", pmsg.from );
	boost::replace_all( preamble, "%n", pmsg.whom );
	return preamble;
}

static std::string	preamble_formater(std::string preamble_xmpp_fmt,  std::string who, std::string room )
{
	// 格式化神器, 哦耶.
	// 获取格式化描述字符串.
	std::string preamble = preamble_xmpp_fmt;
	// 支持的格式化类型有 %u UID,  %q QQ号, %n 昵称,  %c 群名片 %a 自动 %r irc 房间.

	boost::replace_all( preamble, "%a", who );
	boost::replace_all( preamble, "%r", room );
	boost::replace_all( preamble, "%n", who );
	return preamble;
}

static std::string room_name( const avbot::av_message_tree& message )
{
	try{
		if ( message.get<std::string>("protocol") == "mail")
			return "mail";
		if (message.get<std::string>("protocol")== "qq")
			return message.get<std::string>("protocol") + ":" + message.get<std::string>("room.groupnumber");
		return message.get<std::string>("protocol") + ":" + message.get<std::string>("room");
	}catch (...){
		return "none";
	}
}

avbot::avbot( boost::asio::io_service& io_service )
  : m_io_service(io_service), fetch_img(false)
{
	preamble_irc_fmt = "%a 说：";
	preamble_qq_fmt = "qq(%a)说：";
	preamble_xmpp_fmt = "(%a)说：";

	on_message.connect(boost::bind(&avbot::forward_message, this, _1));
}

void avbot::add_to_channel( std::string channel_name, std::string room_name )
{
	if( m_channels.find( channel_name ) != m_channels.end() )
	{
		av_chanel_map c = m_channels[channel_name];

		if( std::find( c.begin(), c.end(), room_name ) == c.end() )
		{
			m_channels[channel_name].push_back( room_name );
		}
	}
	else
	{
		av_chanel_map c;
		c.push_back( room_name );
		m_channels.insert( std::make_pair( channel_name, c ) );
		signal_new_channel(channel_name);
	}
}

std::string avbot::get_channel_name( std::string room_name )
{
	typedef std::pair<std::string, av_chanel_map> avbot_channel_item;
	BOOST_FOREACH(avbot_channel_item c, m_channels)
	{
		if (std::find(c.second.begin(), c.second.end(), room_name)!=c.second.end())
		{
			return c.first;
		}
	}
	return "none";
}

void avbot::broadcast_message( std::string msg)
{
	typedef std::pair<std::string, av_chanel_map> avbot_channel_item;

	// 广播消息到所有的频道.
	BOOST_FOREACH(avbot_channel_item c, m_channels)
	{
		// 好，发消息!
		broadcast_message(c.first, msg);
	}
}

void avbot::broadcast_message( std::string channel_name, std::string msg )
{
	broadcast_message(channel_name, "", msg);
}

void avbot::broadcast_message(std::string channel_name, std::string exclude_room, std::string msg )
{
	BOOST_FOREACH(std::string chatgroupmember, m_channels[channel_name])
	{
		if (chatgroupmember == exclude_room)
			continue;
		if (chatgroupmember.substr(0,3) == "irc")
		{
			if (m_irc_account)
				m_irc_account->chat(std::string("#") + chatgroupmember.substr(4), msg);
		}
		else if (chatgroupmember.substr(0,4) == "xmpp")
		{
			//XMPP
			if (m_xmpp_account)
				m_xmpp_account->send_room_message(chatgroupmember.substr(5), msg);
		} else if (chatgroupmember.substr(0,2)=="qq" )
		{
			if (!m_qq_account)
				continue;
			std::string qqnum = chatgroupmember.substr(3);

			if(m_qq_account->get_Group_by_qq(qqnum))
			{
				m_qq_account->send_group_message(
					*m_qq_account->get_Group_by_qq(qqnum),
					msg, boost::lambda::constant(0)
				);
			}
		}
	}
}


void avbot::callback_on_irc_message( irc::irc_msg pMsg )
{
	using boost::property_tree::ptree;
	// formate irc message to JSON and call on_message

	ptree message;
	message.put("protocol", "irc");
	message.put("room", pMsg.from.substr(1));
	message.put("who.nick", pMsg.whom);
	message.put("channel", get_channel_name(std::string("irc:") + pMsg.from.substr(1)));
	message.put("preamble", preamble_formater( preamble_irc_fmt, pMsg ));

	ptree textmsg;
	textmsg.add("text", pMsg.msg);
	message.add_child("message", textmsg);

	// TODO 将 识别的 URL 进行转化.

	on_message(message);
}

void avbot::callback_on_qq_group_message( std::string group_code, std::string who, const std::vector<webqq::qqMsg >& msg )
{
	using boost::property_tree::ptree;
	ptree message;
	message.put("protocol", "qq");
	ptree ptreee_group;
	ptreee_group.add("code", group_code);

	webqq::qqGroup_ptr group = m_qq_account->get_Group_by_gid( group_code );
	std::string groupname = group_code;

	if( group ){
		groupname = group->name;
		ptreee_group.add("groupnumber", group->qqnum);
		ptreee_group.add("name", group->name);
		message.put("channel", get_channel_name(std::string("qq:") + group->qqnum));
	}else {
		message.put("channel", group_code);
	}

	message.add_child("room", ptreee_group);

	ptree ptree_who;
	ptree_who.add("code", who);

	webqq::qqBuddy_ptr buddy;

	if (group)
		buddy = group->get_Buddy_by_uin( who );

	if (buddy){
		ptree_who.add("nick", buddy->nick.empty()? buddy->uin : buddy->nick) ;
		ptree_who.add("name", buddy->nick);
		ptree_who.add("qqnumber", buddy->qqnum);
		ptree_who.add("card", buddy->card);
		if( ( buddy->mflag & 1 ) == 1 || buddy->uin == group->owner )
			message.add("op", "1");
		else
			message.add("op", "0");
	}else{
		ptree_who.add("nick", who);
	}


	message.add_child("who", ptree_who);
	ptree textmsg;

	std::string message_preamble = preamble_formater(preamble_qq_fmt, buddy.get(), who, group.get() );
	message.add("preamble", message_preamble);

	// 解析 qqMsg
	BOOST_FOREACH( webqq::qqMsg qqmsg, msg )
	{
		std::string buf;

		switch( qqmsg.type ) {
			case webqq::qqMsg::LWQQ_MSG_TEXT:
			{
				textmsg.add("text", qqmsg.text);
			}
			break;
			case webqq::qqMsg::LWQQ_MSG_CFACE:
			{
				if (fetch_img){
					// save to disk
					// 先检查这样的图片本地有没有，有你fetch的P啊.
					fs::path imgfile = fs::path("images") / image_subdir_name(qqmsg.cface.name) / qqmsg.cface.name;
					if (!fs::exists(imgfile))
					{
						if (!fs::exists("images"))
							fs::create_directories("images");
						// 如果顶层目录已经有了的话 ... ...
						fs::path oldimgfile = fs::path("images") / qqmsg.cface.name;
						if (!fs::exists(imgfile.parent_path()))
							fs::create_directories(imgfile.parent_path());
						if (fs::exists(oldimgfile)){
							boost::system::error_code ec;
							fs::create_hard_link(oldimgfile, imgfile, ec);
							if (ec)
								fs::copy(oldimgfile, imgfile);
						}else{
							// 在下载前,  先写入个空白的文件比较好,  这样就算下载的时候崩溃或者推出, 下次启动文件还是会被继续下载的.
							boost::system::error_code ec;
							boost::asio::streambuf buf;

							webqq::webqq::async_fetch_cface_std_saver(ec, buf, qqmsg.cface.name, imgfile.parent_path());
							webqq::webqq::async_fetch_cface(m_io_service, qqmsg.cface, boost::bind(&webqq::webqq::async_fetch_cface_std_saver, _1, _2, qqmsg.cface.name, imgfile.parent_path()));
						}
					}
				}
				// 接收方，需要把 cfage 格式化为 url , loger 格式化为 ../images/XX ,
				// 而 forwarder 则格式化为 http://http://w.qq.com/cgi-bin/get_group_pic?pic=XXX
				textmsg.add("cface.name", qqmsg.cface.name);
				textmsg.add("cface.gid", qqmsg.cface.gid);
				textmsg.add("cface.uin", qqmsg.cface.uin);
				textmsg.add("cface.key", qqmsg.cface.key);
				textmsg.add("cface.server", qqmsg.cface.server);
				textmsg.add("cface.file_id", qqmsg.cface.file_id);
				textmsg.add("cface.vfwebqq", qqmsg.cface.vfwebqq);
				textmsg.add("cface.gchatpicurl", qqmsg.cface.gchatpicurl);
			}
			break;
			case webqq::qqMsg::LWQQ_MSG_FACE:
			{
 				std::string url = boost::str( boost::format("http://0.web.qstatic.com/webqqpic/style/face/%d.gif" ) % qqmsg.face );
 				textmsg.add("img", url);
			} break;
		}
	}
	message.add_child("message", textmsg);
	on_message(message);
}

void avbot::callback_on_xmpp_group_message( std::string xmpproom, std::string who, std::string msg )
{
	using boost::property_tree::ptree;
	ptree message;

	message.put("protocol", "xmpp");
	message.put("channel", get_channel_name(std::string("xmpp:") + xmpproom));
	message.put("room", xmpproom);
	message.put("who.nick", who);
	message.put("preamble", preamble_formater( preamble_xmpp_fmt, who, xmpproom ));

	ptree textmsg;
	textmsg.add("text", msg);
	message.add_child("message", textmsg);

	on_message(message);
}

void avbot::callback_on_mail(mailcontent mail, mx::pop3::call_to_continue_function call_to_contiune )
{
	m_io_service.post(boost::asio::detail::bind_handler(call_to_contiune, m_qq_account->is_online()));

	using boost::property_tree::ptree;
	ptree message;
	message.add("protocol", "mail");
	message.put("channel", get_channel_name(std::string("mail")));

	message.add("from", mail.from);
	message.add("to", mail.to);
	message.add("subject", mail.subject);
	ptree textmessage;
	textmessage.add(mail.content_type, mail.content);
	message.add_child("message", textmessage);
}

void avbot::callback_on_qq_group_found(webqq::qqGroup_ptr group)
{
	// 先检查 QQ 群有没有被加入过，没有再新加入个吧.
	if (get_channel_name(std::string("qq:")+group->qqnum)=="none")
		add_to_channel(group->qqnum, std::string("qq:") + group->qqnum);
}

void avbot::callback_on_qq_group_newbee(webqq::qqGroup_ptr group, webqq::qqBuddy_ptr buddy)
{
	// 新人入群咯.
	if (get_channel_name(std::string("qq:")+group->qqnum)=="none")
		return;

	// 构造 json 消息,  格式同 QQ 消息, 就是多了个 newbee 字段

	using boost::property_tree::ptree;
	ptree message;
	message.put("protocol", "qq");
	ptree ptreee_group;
	ptreee_group.add("code", group->code);

	std::string groupname = group->name;

	ptreee_group.add("groupnumber", group->qqnum);
	ptreee_group.add("name", group->name);

	message.put("channel", get_channel_name(std::string("qq:") + group->qqnum));
	message.add_child("room", ptreee_group);

	ptree ptree_who;
	if (buddy){
		ptree_who.add("code", buddy->uin);

		ptree_who.add("name", buddy->nick);
		ptree_who.add("qqnumber", buddy->qqnum);
		ptree_who.add("card", buddy->card);
		ptree_who.add("nick", buddy->card.empty()? buddy->nick:buddy->card);
		if( ( buddy->mflag & 21 ) == 21 || buddy->uin == group->owner )
			message.add("op", "1");
		else
			message.add("op", "0");
		message.add("newbee", buddy->uin);
	}else{
		// 新人入群,  可是 webqq 暂时无法获取新人昵称.

		ptree_who.add("nick", "获取名字失败");

		return;
	}

	message.add_child("who", ptree_who);
	ptree textmsg;

	message.add("preamble", "群系统消息: ");

	if (buddy){
		message.add("message.text", boost::str(boost::format("新人 %s 入群.") % buddy->nick ));
	}else{
		message.add("message.text", "新人入群,  可是 webqq 暂时无法获取新人昵称.");
	}

	on_message(message);
}

void avbot::set_qq_account( std::string qqnumber, std::string password, avbot::need_verify_image cb, bool no_persistent_db)
{
	m_qq_account.reset(new webqq::webqq(m_io_service, qqnumber, password, no_persistent_db));
	m_qq_account->on_verify_code(cb);
	m_qq_account->on_group_msg(boost::bind(&avbot::callback_on_qq_group_message, this, _1, _2, _3));
	m_qq_account->on_group_found(boost::bind(&avbot::callback_on_qq_group_found, this, _1));
	m_qq_account->on_group_newbee(boost::bind(&avbot::callback_on_qq_group_newbee, this, _1, _2));
}

void avbot::relogin_qq_account()
{
// 	m_qq_account->login();
}

void avbot::feed_login_verify_code( std::string vcode, boost::function<void()> badvcreporter)
{
	if (!m_qq_account->is_online())
		m_qq_account->feed_vc(vcode, badvcreporter);
}

void avbot::set_irc_account( std::string nick, std::string password, std::string server, bool use_ssl)
{
	boost::cmatch what;
	if (use_ssl){
		boost::throw_exception(std::invalid_argument("ssl is currently not supported"));
	}
	m_irc_account.reset(new irc::client(m_io_service, nick, password, server));
	m_irc_account->on_privmsg_message(boost::bind(&avbot::callback_on_irc_message, this, _1));
}

void avbot::irc_join_room( std::string room_name )
{
	if (m_irc_account)
		m_irc_account->join(room_name);
}

void avbot::set_xmpp_account( std::string user, std::string password, std::string nick, std::string server )
{
	m_xmpp_account.reset(new xmpp(m_io_service, user, password, server, nick));
	m_xmpp_account->on_room_message(boost::bind(&avbot::callback_on_xmpp_group_message, this, _1, _2, _3));
}

void avbot::xmpp_join_room( std::string room )
{
	if (m_xmpp_account)
		m_xmpp_account->join(room);
}

void avbot::set_mail_account( std::string mailaddr, std::string password, std::string pop3server, std::string smtpserver )
{
	// 开启 pop3 收邮件.
	m_mail_account.reset(new mx::mx(m_io_service, mailaddr, password, pop3server, smtpserver));

	m_mail_account->async_fetch_mail(boost::bind(&avbot::callback_on_mail, this, _1, _2));
}

void avbot::forward_message( const boost::property_tree::ptree& message )
{
	std::string channel_name = message.get<std::string>( "channel" );
	// 根据 channels 的配置，执行消息转发.
	// 转发前格式化消息.

	std::string formated = format_message( message );

	if( channel_name.empty() )
	{
		// do broadcast_message
		broadcast_message(formated);
	}
	else
	{
		if( m_channels.find( channel_name ) != m_channels.end() )
		{
			// 好，发消息!
			broadcast_message( channel_name, room_name( message ), formated );
		}
	}
}


std::string avbot::format_message( const avbot::av_message_tree& message )
{
	std::string linermessage;
	// 首先是根据 nick 格式化
	if ( message.get<std::string>("protocol") != "mail")
	{
		linermessage += message.get<std::string>("preamble", "");

		BOOST_FOREACH(const avbot::av_message_tree::value_type & v, message.get_child("message"))
		{
			if (v.first == "text")
			{
				linermessage += v.second.data();
			}else if (v.first == "url"||v.first == "img")
			{
				linermessage += " ";
				linermessage += v.second.data();
				linermessage += " ";
			}else if (v.first == "cface"){
				// 执行 HTTP 访问获得 302 跳转后的 URL.
				linermessage += v.second.get<std::string>("gchatpicurl");
				linermessage += " ";
			}
		}
	}else{

		linermessage  = boost::str(
			boost::format( "[QQ邮件]\n发件人:%s\n收件人:%s\n主题:%s\n\n%s" )
			% message.get<std::string>("from") % message.get<std::string>("to") % message.get<std::string>("subject")
			% message.get_child("message").data()
		);
 	}

 	return linermessage;
}

std::string avbot::image_subdir_name( std::string cface )
{
	boost::replace_all( cface, "{", "" );
	boost::replace_all( cface, "}", "" );
	boost::replace_all( cface, "-", "" );
	return cface.substr(0, 2);
}
