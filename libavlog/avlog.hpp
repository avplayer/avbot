#pragma once

#include <boost/noncopyable.hpp>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <boost/date_time.hpp>

class avlog : public boost::noncopyable
{
public:
	typedef boost::shared_ptr<std::ofstream> ofstream_ptr;
	typedef std::map<std::string, ofstream_ptr> loglist;

	static std::string html_escape(std::string);
public:
	std::string log_path()
	{
		return  m_path.string() ;
	}
	// 设置日志保存路径.
	void log_path( const std::wstring &path )
	{
		m_path = path;
	}
	// 设置日志保存路径.
	void log_path( const std::string &path )
	{
		m_path = path;
	}
	// 添加日志消息.
	bool add_log( const std::string &groupid, const std::string &msg );

	// 开始讲座
	bool begin_lecture( const std::string &groupid, const std::string &title )
	{
		// 已经打开讲座, 乱调用API, 返回失败!
		if( m_lecture_file ) return false;

		// 构造讲座文件生成路径.
		std::string save_path = make_path( groupid ) + "/" + title + ".html";

		// 创建文件.
		m_lecture_file.reset( new std::ofstream( save_path.c_str(),
							  fs::exists( save_path ) ? std::ofstream::app : std::ofstream::out ) );

		if( m_lecture_file->bad() || m_lecture_file->fail() )
		{
			std::cerr << "create file " << save_path.c_str() << " failed!" << std::endl;
			return false;
		}

		// 保存讲座群的id.
		m_lecture_groupid = groupid;

		// 写入信息头.
		std::string info = "<head><meta http-equiv=\"Content-Type\" content=\"text/plain; charset=UTF-8\">\n";
		m_lecture_file->write( info.c_str(), info.length() );
		std::string htmltitle = std::string("<title>") + title + "</title>\r\n" ;
		m_lecture_file->write( htmltitle.c_str(), htmltitle.length() );

		return true;
	}

	// 讲座结束.
	void end_lecture()
	{
		// 清空讲座群id.
		m_lecture_groupid.clear();
		// 重置讲座文件指针.
		m_lecture_file.reset();
	}

protected:

	// 构造路径.
	std::string make_path( const std::string &groupid ) const
	{
		return ( m_path / groupid ).string();
	}

	// 构造文件名.
	std::string make_filename( const std::string &p = "" ) const
	{
		std::ostringstream oss;
		boost::posix_time::time_facet* _facet = new boost::posix_time::time_facet( "%Y%m%d" );
		oss.imbue( std::locale( std::locale::classic(), _facet ) );
		oss << boost::posix_time::second_clock::local_time();
		std::string filename = ( fs::path( p ) / ( oss.str() + ".html" ) ).string();
		return filename;
	}

	// 创建对应的日志文件, 返回日志文件指针.
	ofstream_ptr create_file( const std::string &groupid ) const;

public:
	// 得到当前时间字符串, 对应printf格式: "%04d-%02d-%02d %02d:%02d:%02d"
	static std::string current_time()
	{
		std::ostringstream oss;
		boost::posix_time::time_facet* _facet = new boost::posix_time::time_facet( "%Y-%m-%d %H:%M:%S%F" );
		oss.imbue( std::locale( std::locale::classic(), _facet ) );
		oss << boost::posix_time::second_clock::local_time();
		std::string time = oss.str();
		return time;
	}

private:
	ofstream_ptr m_lecture_file;
	std::string m_lecture_groupid;
	loglist m_group_list;
	fs::path m_path;
};
