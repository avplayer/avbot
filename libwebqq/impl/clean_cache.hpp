#pragma once

#include <string>

#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <boost/avloop.hpp>
#include <boost/filesystem.hpp>
#include <boost/async_dir_walk.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/timedcall.hpp>

namespace webqq{
namespace detail
{

void clean_cache_dir_walk_handler( boost::asio::io_service & io_service, const boost::filesystem::path & item, boost::async_dir_walk_continue_handler handler )
{
	using namespace boost::system;

	//boost::function<void(const boost::system::error_code&)> h = io_service.wrap(handler);
	if( boost::filesystem::is_regular_file( item ) )
	{
		boost::regex ex( "group_.*" );
		boost::cmatch what;

		std::string filename  =  boost::filesystem::basename( item );

		if( boost::regex_match( filename.c_str(), what, ex ) )
		{
			boost::posix_time::ptime now = boost::posix_time::from_time_t( std::time( 0 ) );

			// 执行 glob 匹配,  寻找 cache_*
			// 删除最后访问时间超过一天的文件.

			boost::posix_time::ptime last_write_time
				= boost::posix_time::from_time_t( boost::filesystem::last_write_time( item ) );

			if( (now.date() - last_write_time.date()).days() > 0 )
			{
				// TODO! 应该集中到一个地方一起 remove ! 对吧!
				boost::filesystem::remove( item );
			}
		}
	}

	handler( error_code() );
}

}

inline void clean_cache( boost::asio::io_service &io_service);
inline void clean_cache( boost::asio::io_service &io_service, boost::system::error_code ec)
{
	boost::delayedcallsec(io_service, 4000, boost::bind( &clean_cache, boost::ref( io_service )));
}

inline void clean_cache( boost::asio::io_service &io_service)
{
	boost::async_dir_walk( io_service, boost::filesystem::path( "cache" ),
							boost::bind( detail::clean_cache_dir_walk_handler, boost::ref( io_service ), _1, _2 ),
							boost::bind( &clean_cache, boost::ref( io_service ), _1)
						 );
}

} // namespace webqq
