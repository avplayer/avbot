#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/format.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/regex.hpp>

#include <iostream>
#include <boost/foreach.hpp>
#include <boost/variant.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/locale.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include "internet_mail_format.hpp"
#include <boost/base64.hpp>

#include "smtp.hpp"

static void sended( const boost::system::error_code & ec ,boost::asio::io_service* io)
{
	std::cout <<  ec.message() <<  std::endl;
	io->stop();

#include "fsconfig.ipp"

int main( int argc, char * argv[] )
{
	std::string qqnumber, qqpwd;
	std::string ircnick, ircroom, ircpwd;
	std::string xmppuser, xmppserver, xmpppwd, xmpproom;
	std::string cfgfile;
	std::string logdir;
	std::string chanelmap;
	std::string mailaddr, mailpasswd, mailserver;

	setlocale( LC_ALL, "" );

	po::options_description desc( "qqbot options" );
	desc.add_options()
	( "version,v",										"output version" )
	( "help,h",											"produce help message" )
	( "daemon,d",										"go to background" )
	( "mail",		po::value<std::string>( &mailaddr ),	"send mail by this address" )
	( "mailpasswd",	po::value<std::string>( &mailpasswd ), "password of mail" )
	( "mailserver",	po::value<std::string>( &mailserver ), "server to use" )
	;

	po::variables_map vm;
	po::store( po::parse_command_line( argc, argv, desc ), vm );
	po::notify( vm );

	if( vm.count( "help" ) ) {
		std::cerr <<  desc <<  std::endl;
		return 1;
	}

	if( vm.size() == 0 ) {
		try {
			fs::path p = configfilepath();
			po::store( po::parse_config_file<char>( p.string().c_str(), desc ), vm );
			po::notify( vm );
		} catch( char* e ) {
			std::cerr << e << std::endl;
		}
	}

	boost::asio::io_service io;
	boost::asio::io_service::work word( io );
	mx::smtp smtp( io, mailaddr, mailpasswd, mailserver );
	InternetMailFormat imf;

	imf.header["from"] = mailaddr;
	imf.header["to"] = "\"晕菜\" <beansoft@qq.com>";
	imf.header["subject"] = "test mail";
	imf.header["content-type"] = "text/plain; charset=utf8";

	imf.body = "test body dasdfasd ";
	std::stringstream maildata;
	boost::asio::streambuf buf;
	std::ostream os( &buf );
	imf_write_stream( imf, os );

	std::string _mdata = boost::asio::buffer_cast<const char*>( buf.data() );

	smtp.async_sendmail( imf, boost::bind( sended,_1,&io ) );

	avloop_run(io);
}
