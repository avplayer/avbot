
/**
 * @file   main.cpp
 * @author microcai <microcaicai@gmail.com>
 *
 */
#include <unistd.h>
#include <string>
#include <algorithm>
#include <vector>
#include <signal.h>
#include <fstream>

#include <boost/log/trivial.hpp>

#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include <boost/program_options.hpp>
namespace po = boost::program_options;
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/noncopyable.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/locale.hpp>
#include <boost/signals2.hpp>
#include <boost/lambda/lambda.hpp>
#include <locale.h>
#include <cstring>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>

#include <avhttp.hpp>

#include "boost/consolestr.hpp"
#include "boost/acceptor_server.hpp"
#include "boost/avloop.hpp"

#include "libavbot/avbot.hpp"
#include "libavlog/avlog.hpp"

#include "counter.hpp"

#include "botctl.hpp"
#include "input.hpp"
#include "avbot_vc_feed_input.hpp"
#include "avbot_rpc_server.hpp"

#include "extension/extension.hpp"
#include "deCAPTCHA/decaptcha.hpp"
#include "deCAPTCHA/deathbycaptcha_decoder.hpp"
#include "deCAPTCHA/channel_friend_decoder.hpp"
#include "deCAPTCHA/antigate_decoder.hpp"
#include "deCAPTCHA/avplayer_free_decoder.hpp"
#include "deCAPTCHA/jsdati_decoder.hpp"
#include "deCAPTCHA/hydati_decoder.hpp"

#ifndef QQBOT_VERSION
#ifdef PACKAGE_VERSION
#   define QQBOT_VERSION PACKAGE_VERSION
#   else
#	define QQBOT_VERSION "unknow"
#   endif
#endif

char * execpath;
avlog logfile;			// 用于记录日志文件.

static counter cnt;				// 用于统计发言信息.
static std::string progname;
static bool need_vc = false;
static std::string preamble_qq_fmt, preamble_irc_fmt, preamble_xmpp_fmt;

static void vc_code_decoded(boost::system::error_code ec, std::string provider, std::string vccode, boost::function<void()> reportbadvc, avbot & mybot)
{
	BOOST_LOG_TRIVIAL(info) << console_out_str("使用 ") <<  console_out_str(provider) << console_out_str(" 成功解码验证码!");

	if (provider == "IRC/XMPP 好友辅助验证码解码器")
		mybot.broadcast_message("验证码已输入");

	mybot.feed_login_verify_code(vccode, reportbadvc);
	need_vc = false;
}

static void on_verify_code(const boost::asio::const_buffer & imgbuf, avbot & mybot, decaptcha::deCAPTCHA & decaptcha)
{
	const char * data = boost::asio::buffer_cast<const char*>( imgbuf );
	size_t	imgsize = boost::asio::buffer_size( imgbuf );
	std::string buffer(data, imgsize);

	BOOST_LOG_TRIVIAL(info) << "got vercode from TX, now try to auto resovle it ... ...";

	need_vc = true;
	// 保存文件.
	std::ofstream	img("vercode.jpeg", std::ofstream::openmode(std::ofstream::binary | std::ofstream::out) );
	img.write(data, imgsize);
	img.close();

	decaptcha.async_decaptcha(
		buffer,
		boost::bind(&vc_code_decoded, _1, _2, _3, _4, boost::ref(mybot))
	);
}

static void stdin_feed_broadcast(avbot & mybot, std::string line_input)
{
	if (need_vc){
		return;
	}

	boost::trim_right(line_input);

	if(line_input.size() > 1)
	{
		mybot.broadcast_message(
			boost::str(
				boost::format("来自 avbot 命令行的消息: %s") % line_input
			)
		);
	}
}


struct build_group_has_qq{
	bool operator()(const std::string & str)
	{
		return str.substr(0, 3) == "qq:";
	}
};

static void build_group(std::string chanelmapstring, avbot & mybot)
{
	std::vector<std::string> gs;
	boost::split(gs, chanelmapstring, boost::is_any_of(";"));

	int m = 1;

	BOOST_FOREACH(std::string  pergroup, gs)
	{
		std::vector<std::string> groups;
		boost::split(groups, pergroup, boost::is_any_of(","));

		// 对于包含了 qq: 的那个组，组名就是 qq号码，而其他的嘛，组名使用 groupXXX
		std::vector<std::string>::iterator qqgroup = std::find_if(groups.begin(), groups.end(), build_group_has_qq() );

		if (qqgroup != groups.end())
		{
			BOOST_FOREACH(std::string  c, groups)
			{
				mybot.add_to_channel((*qqgroup).substr(3), c);
			}
		}else{
			m ++;
			std::string group_name = std::string("group") + boost::lexical_cast<std::string>(m);
			BOOST_FOREACH(std::string  c, groups)
			{
				mybot.add_to_channel(group_name, c);
			}
		}
	}
}

static void avbot_log( avbot::av_message_tree message, avbot & mybot )
{
	std::string linemessage;

	// 首先是根据 nick 格式化
	if( message.get<std::string>( "protocol" ) != "mail" )
	{
		linemessage += message.get<std::string>( "preamble", "" );

		BOOST_FOREACH( const avbot::av_message_tree::value_type & v, message.get_child( "message" ) )
		{
			if( v.first == "text" )
			{
				linemessage += avlog::html_escape(v.second.data());
			}
			else if( v.first == "url" )
			{
				linemessage += boost::str( boost::format( "<a href=\"%s\">%s</a>" ) % v.second.data() % v.second.data() );
			}
			else if( v.first == "cface" )
			{
				if( mybot.fetch_img ){
					std::string cface = v.second.get<std::string>("name");
					linemessage += boost::str( boost::format( "<img src=\"../images/%s/%s\" />" ) % avbot::image_subdir_name(cface) % cface );
				}
				else{

					std::string url = v.second.get<std::string>("gchatpicurl");
					linemessage += boost::str( boost::format( "\t\t<img src=\"%s\" />\r\n" ) % url );
				}
			}
			else if( v.first == "img" )
			{
				linemessage += boost::str( boost::format( "\t\t<img src=\"%s\" />\r\n" ) % v.second.data() );
			}
		}
		std::string channel_name = message.get<std::string>( "channel", "" );

		if( channel_name.empty() )
		{
			// 全部记录?
			// TODO
		}
		else
		{
			// 如果最好的办法就是遍历组里的所有QQ群，都记录一次.
			avbot::av_chanel_map channelmap = mybot.get_channel_map( channel_name );

			// 如果没有Q群，诶，只好，嘻嘻.
			logfile.add_log( channel_name, linemessage );

			BOOST_FOREACH( std::string roomname, channelmap )
			{
				if( roomname.substr( 0, 3 ) == "qq:" )
				{
					// 避免重复记录.
					if( roomname.substr( 3 ) != channel_name )
					{
						logfile.add_log( roomname.substr( 3 ), linemessage );
					}
				}
			}
		}
	}
	else
	{
		// TODO ,  暂时不记录邮件吧！

// 		mybot.get_channel_name("room");
//
// 		linemessage  = boost::str(
// 			boost::format( "[QQ邮件]\n发件人:%s\n收件人:%s\n主题:%s\n\n%s" )
// 			% message.get<std::string>("from") % message.get<std::string>("to") % message.get<std::string>("subject")
// 			% message.get_child("message").data()
// 		);
	}
}

static void avbot_rpc_server(boost::shared_ptr<boost::asio::ip::tcp::socket> m_socket, avbot & mybot)
{
	::detail::avbot_rpc_server(m_socket, mybot.on_message);
}

static void my_on_bot_command(avbot::av_message_tree message, avbot & mybot)
{
	try{
		std::string textmessage;
		try{
			textmessage = message.get<std::string>("newbee");
			// 如果没有发生异常, 那就是新人入群消息了, 嘻嘻.
			// 格式化为 .qqbot newbee XXX, 哼!
			std::string nick = message.get<std::string>("who.name");
			textmessage = boost::str(boost::format(".qqbot newbee %s") % nick);
			message.put("message.text", textmessage);
			message.put("op", 1);

			boost::delayedcallsec(mybot.get_io_service(), 6, boost::bind(on_bot_command, message, boost::ref(mybot)));
			return;
		}catch (const boost::property_tree::ptree_bad_path &){
			textmessage = message.get<std::string>("message.text");
		}

		on_bot_command(message, mybot);
	}catch (...){}

}

#ifdef WIN32
int daemon( int nochdir, int noclose )
{
	// nothing...
	return -1;
}
#endif // WIN32

#include "fsconfig.ipp"

int main( int argc, char *argv[] )
{
	std::string qqnumber, qqpwd;
	std::string ircnick, ircroom, ircpwd;
	std::string xmppuser, xmppserver, xmpppwd, xmpproom, xmppnick;
	std::string cfgfile;
	std::string logdir;
	std::string chanelmap;
	std::string mailaddr, mailpasswd, pop3server, smtpserver;
	std::string jsdati_username, jsdati_password;
	std::string hydati_key;
	std::string deathbycaptcha_username, deathbycaptcha_password;
	//http://api.dbcapi.me/in.php
	//http://antigate.com/in.php
	std::string antigate_key, antigate_host;
	bool use_avplayer_free_vercode_decoder(false);

	fs::path config; // 配置文件的路径

	unsigned rpcport;

	boost::asio::io_service io_service;

	avbot mybot(io_service);

	progname = fs::basename( argv[0] );

	setlocale( LC_ALL, "" );
	po::variables_map vm;
	po::options_description desc( "qqbot options" );
	desc.add_options()
	( "version,v", 	"output version" )
	( "help,h", 	"produce help message" )
	( "daemon,d", 	"go to background" )
#ifdef WIN32
	( "gui,g",	 	"pop up settings dialog" )
#endif

	( "config,c",	po::value<fs::path>( &config ),			"use an alternative configuration file." )
	( "qqnum,u",	po::value<std::string>( &qqnumber ), 	"QQ number" )
	( "qqpwd,p",	po::value<std::string>( &qqpwd ), 		"QQ password" )
	( "logdir",		po::value<std::string>( &logdir ), 		"dir for logfile" )
	( "ircnick",	po::value<std::string>( &ircnick ), 	"irc nick" )
	( "ircpwd",		po::value<std::string>( &ircpwd ), 		"irc password" )
	( "ircrooms",	po::value<std::string>( &ircroom ), 	"irc room" )
	( "xmppuser",	po::value<std::string>( &xmppuser ), 	"id for XMPP,  eg: (microcaicai@gmail.com)" )
	( "xmppserver",	po::value<std::string>( &xmppserver ), 	 "server to connect for XMPP,  eg: (xmpp.l.google.com)" )
	( "xmpppwd",	po::value<std::string>( &xmpppwd ), 	"password for XMPP" )
	( "xmpprooms",	po::value<std::string>( &xmpproom ), 	"xmpp rooms" )
	( "xmppnick",	po::value<std::string>( &xmppnick ), 	"nick in xmpp rooms" )
	( "map",		po::value<std::string>( &chanelmap ), 	"map channels. eg: --map=qq:12345,irc:avplayer;qq:56789,irc:ubuntu-cn" )
	( "mail",		po::value<std::string>( &mailaddr ), 	"fetch mail from this address" )
	( "mailpasswd",	po::value<std::string>( &mailpasswd ), 	"password of mail" )
	( "pop3server",	po::value<std::string>( &pop3server ), 	"pop server of mail,  default to pop.[domain]" )
	( "smtpserver",	po::value<std::string>( &smtpserver ), 	"smtp server of mail,  default to smtp.[domain]" )

	( "jsdati_username", po::value<std::string>( &jsdati_username ),	console_out_str("联众打码服务账户").c_str() )
	( "jsdati_password", po::value<std::string>( &jsdati_password ),	console_out_str("联众打码服务密码").c_str() )

	( "hydati_key", po::value<std::string>( &hydati_key ),	console_out_str("慧眼答题服务key").c_str() )

	( "deathbycaptcha_username", po::value<std::string>( &deathbycaptcha_username ),	console_out_str("阿三解码服务账户").c_str() )
	( "deathbycaptcha_password", po::value<std::string>( &deathbycaptcha_password ),	console_out_str("阿三解码服务密码").c_str() )

	( "antigate_key", po::value<std::string>( &antigate_key ),	console_out_str("antigate解码服务key").c_str() )
	( "antigate_host", po::value<std::string>( &antigate_host )->default_value("http://antigate.com/"),	console_out_str("antigate解码服务器地址").c_str() )

	( "use_avplayer_free_vercode_decoder", po::value<bool>( &use_avplayer_free_vercode_decoder ), "don't use" )

	( "localimage", po::value<bool>( &(mybot.fetch_img) )->default_value(true),	"fetch qq image to local disk and store it there")
	( "rpcport",	po::value<unsigned>(&rpcport)->default_value(6176),				"run rpc server on port 6176")

	( "preambleqq",		po::value<std::string>( &preamble_qq_fmt )->default_value( console_out_str("qq(%a) :") ),
		console_out_str("为QQ设置的发言前缀, 默认是 qq(%a):").c_str() )
	( "preambleirc",	po::value<std::string>( &preamble_irc_fmt )->default_value( console_out_str("%a 说 :") ),
	  console_out_str("为IRC设置的发言前缀, 默认是 %a 说: ").c_str() )
	( "preamblexmpp",	po::value<std::string>( &preamble_xmpp_fmt )->default_value( console_out_str("(%a) :") ),
		console_out_str(
	  "为XMPP设置的发言前缀, 默认是 (%a): \n\n "
	  "前缀里的含义 \n"
	  "\t %a 为自动选择\n\t %q 为QQ号码\n\t %n 为昵称\n\t %c 为群名片 \n"
	  "\t %r为房间名(群号, XMPP房名, IRC频道名) \n"
	  "可以包含多个, 例如想记录QQ号码的可以使用 qq(%a, %q)说: \n"
	  "注意在shell下可能需要使用\\(来转义(\n配置文件无此问题 \n\n").c_str()	)
	;

	po::store( po::parse_command_line( argc, argv, desc ), vm );
	po::notify( vm );

	if( vm.count( "help" ) ) {
		std::cerr <<  desc <<  std::endl;
		return 1;
	}
#ifdef WIN32
	// 从 windows 控制台输入的能有啥好编码，转到utf8吧.
	preamble_qq_fmt = ansi_utf8(preamble_qq_fmt);
	preamble_irc_fmt = ansi_utf8(preamble_irc_fmt);
	preamble_xmpp_fmt = ansi_utf8(preamble_xmpp_fmt);
#endif

	if( qqnumber.empty() ) {
		// 命令行没指定选项，读取配置文件.
		try {
			if (config.empty()) {
				// 命令行没指定配置文件？使用默认的!
				config = configfilepath();
			}
			BOOST_LOG_TRIVIAL(info) << "loading config from: " << config.string();
			po::store( po::parse_config_file<char>( config.string().c_str(), desc ), vm );
			po::notify( vm );
		} catch( char const* e ) {
			BOOST_LOG_TRIVIAL(fatal) <<  "no command line arg and config file not found neither.";
			BOOST_LOG_TRIVIAL(fatal) <<  "try to add command line arg or put config file in /etc/qqbotrc or ~/.qqbotrc";
			exit(1);
		}
	}


	if( vm.count( "daemon" ) ) {
		daemon( 0, 0 );
	}

	if( vm.count( "version" ) ) {
		printf( "qqbot version %s (%s %s) \n", QQBOT_VERSION, __DATE__, __TIME__ );
		exit( EXIT_SUCCESS );
	}

	if( !logdir.empty() ) {
		if( !fs::exists( logdir ) )
			fs::create_directory( logdir );
	}

	// 设置到中国的时区，否则 qq 消息时间不对啊.
#ifndef _WIN32
	setenv("TZ", "Asia/Shanghai", 1);
# else
	putenv( ( char* )"TZ=Asia/Shanghai" );
# endif

#ifdef _WIN32
	if( qqnumber.empty() || qqpwd.empty() || vm.count( "gui" )>0)
	{
		void show_dialog(std::string & qqnumer, std::string & qqpwd, std::string & ircnick,
					std::string & ircroom, std::string & ircpwd,
					std::string &xmppuser, std::string & xmppserver,std::string & xmpppwd, std::string & xmpproom, std::string &xmppnick);

		show_dialog(qqnumber, qqpwd, ircnick, ircroom, ircpwd, xmppuser, xmppserver, xmpppwd, xmpproom,xmppnick);
	}
#endif

	{
#ifndef _WIN32
		std::string env_oldpwd = fs::complete(fs::current_path()).string();
		setenv("O_PWD", env_oldpwd.c_str(), 1);
# else
		std::string env_oldpwd = std::string("O_PWD=") + fs::complete(fs::current_path()).string();
		putenv( (char*)env_oldpwd.c_str() );
#endif

	}
	// 设置日志自动记录目录.
	if( ! logdir.empty() ) {
		logfile.log_path( logdir );
		chdir( logdir.c_str() );
	}

	if( qqnumber.empty() || qqpwd.empty() ) {
		BOOST_LOG_TRIVIAL(fatal) << console_out_str("请设置qq号码和密码.");
		exit( 1 );
	}

	decaptcha::deCAPTCHA decaptcha(io_service);

	avbot_vc_feed_input vcinput(io_service);

	// 连接到 std input
	connect_stdinput(boost::bind(&avbot_vc_feed_input::call_this_to_feed_line, &vcinput, _1));
	mybot.on_message.connect(boost::bind(&avbot_vc_feed_input::call_this_to_feed_message, &vcinput, _1));

	if(!hydati_key.empty())
	{
		decaptcha.add_decoder(
			decaptcha::decoder::hydati_decoder(
				io_service, hydati_key
			)
		);
	}

	if(!jsdati_username.empty() && !jsdati_password.empty())
	{
		decaptcha.add_decoder(
			decaptcha::decoder::jsdati_decoder(
				io_service, jsdati_username, jsdati_password
			)
		);
	}

	if (!deathbycaptcha_username.empty() && !deathbycaptcha_password.empty())
	{
		decaptcha.add_decoder(
			decaptcha::decoder::deathbycaptcha_decoder(
				io_service, deathbycaptcha_username, deathbycaptcha_password
			)
		);
	}

	if(!antigate_key.empty())
	{
		decaptcha.add_decoder(
			decaptcha::decoder::antigate_decoder(io_service, antigate_key, antigate_host)
		);
	}

	if(use_avplayer_free_vercode_decoder)
	{
		decaptcha.add_decoder(
			decaptcha::decoder::avplayer_free_decoder(io_service)
		);
	}

	decaptcha.add_decoder(
		decaptcha::decoder::channel_friend_decoder(
			io_service, boost::bind(&avbot::broadcast_message, &mybot, _1),
			boost::bind(&avbot_vc_feed_input::async_input_read_timeout, &vcinput, 45, _1)
		)
	);

	mybot.preamble_irc_fmt = preamble_irc_fmt;
	mybot.preamble_qq_fmt = preamble_qq_fmt;
	mybot.preamble_xmpp_fmt = preamble_xmpp_fmt;

	mybot.signal_new_channel.connect(boost::bind(new_channel_set_extension, boost::ref(io_service), boost::ref(mybot), _1));

	mybot.set_qq_account( qqnumber, qqpwd, boost::bind( on_verify_code, _1, boost::ref( mybot ), boost::ref(decaptcha) ) );

	if( !ircnick.empty() )
		mybot.set_irc_account( ircnick, ircpwd );

	if( !xmppuser.empty() )
		mybot.set_xmpp_account( xmppuser, xmpppwd, xmppserver, xmppnick );

	if( !mailaddr.empty() )
		mybot.set_mail_account( mailaddr, mailpasswd, pop3server, smtpserver );

	build_group( chanelmap, mybot );
	// 记录到日志.
	mybot.on_message.connect(boost::bind(avbot_log, _1, boost::ref(mybot)));
	// 开启 bot 控制.
 	mybot.on_message.connect(boost::bind(my_on_bot_command, _1, boost::ref(mybot)));

	std::vector<std::string> ircrooms;
	boost::split( ircrooms, ircroom, boost::is_any_of( "," ) );
	BOOST_FOREACH( std::string room , ircrooms )
	{
		mybot.irc_join_room(std::string( "#" ) + room);
	}

	std::vector<std::string> xmpprooms;
	boost::split( xmpprooms, xmpproom, boost::is_any_of( "," ) );
	BOOST_FOREACH( std::string room , xmpprooms )
	{
		mybot.xmpp_join_room(room);
	}

	boost::asio::io_service::work work( io_service );

	if( !vm.count( "daemon" ) ) {
		start_stdinput(io_service);
		connect_stdinput( boost::bind( stdin_feed_broadcast , boost::ref(mybot), _1));
	}

	if (rpcport > 0)
	{
		try
		{
			// 调用 acceptor_server 跑 avbot_rpc_server 。 在端口 6176 上跑哦!
			boost::acceptor_server( io_service,
									boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v6(), rpcport ),
									boost::bind( avbot_rpc_server, _1, boost::ref( mybot ) )
								  );
		}
		catch( ... )
		{
			try
			{
				boost::acceptor_server( io_service,
										boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), rpcport ),
										boost::bind( avbot_rpc_server, _1, boost::ref( mybot ) )
									  );
			}catch(...)
			{
				BOOST_LOG_TRIVIAL(warning) <<  "bind to port " <<  rpcport <<  " failed!";
				BOOST_LOG_TRIVIAL(warning) <<  "Did you happened to already run an avbot? ";
				BOOST_LOG_TRIVIAL(warning) <<  "Now avbot will run without RPC support. ";
			}
		}
	}

	avhttp::http_stream s(io_service);
	s.async_open("https://avlog.avplayer.org/cache/tj.php", boost::bind(&avhttp::http_stream::close, &s) );

	avloop_run( io_service);
	return 0;
}
