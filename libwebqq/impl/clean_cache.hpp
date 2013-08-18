#pragma once

#include <string>

#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <boost/avloop.hpp>
#include <boost/filesystem.hpp>
#include <boost/async_dir_walk.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/timedcall.hpp>

#include "webqq_impl.hpp"

namespace webqq {
namespace qqimpl {
namespace detail {

class clean_cache_op : boost::asio::coroutine
{
public:
	clean_cache_op(boost::shared_ptr<WebQQ> webqq)
		: m_webqq(webqq)
	{
		boost::async_dir_walk(
			m_webqq->get_ioservice(),
			boost::filesystem::path("cache"),
			*this,
			*this
		);
	}

	template<class DirWalkContinueHandler>
	void operator()(const boost::filesystem::path& item, DirWalkContinueHandler handler)
	{
		// 回调在这里,  一次清理一个.
		using namespace boost::system;

		//boost::function<void(const boost::system::error_code&)> h = io_service.wrap(handler);
		if (boost::filesystem::is_regular_file(item))
		{
			boost::regex ex("group_.*");
			boost::cmatch what;

			std::string filename  =  boost::filesystem::basename(item);

			if (boost::regex_match(filename.c_str(), what, ex))
			{
				boost::posix_time::ptime now = boost::posix_time::from_time_t(std::time(0));

				// 执行 glob 匹配,  寻找 cache_*
				// 删除最后访问时间超过一天的文件.

				boost::posix_time::ptime last_write_time
					= boost::posix_time::from_time_t(boost::filesystem::last_write_time(item));

				if ((now.date() - last_write_time.date()).days() > 0)
				{
					// TODO! 应该集中到一个地方一起 remove ! 对吧!
					boost::filesystem::remove(item);
				}
			}
		}

		m_webqq->get_ioservice().post(
			boost::bind<void>(handler, error_code())
		);
	}

	void operator()(boost::system::error_code ec)
	{
		BOOST_ASIO_CORO_REENTER(this)
		{for (;m_webqq->m_status!= LWQQ_STATUS_QUITTING;){
			// 清理完成!
			BOOST_ASIO_CORO_YIELD boost::delayedcallsec(
				m_webqq->get_ioservice(), 4000,
				boost::bind<void>(*this, ec)
			);

			// 重启清理协程.
			BOOST_ASIO_CORO_YIELD boost::async_dir_walk(
				m_webqq->get_ioservice(),
				boost::filesystem::path("cache"),
				*this,
				*this
			);
		}}
	}
private:
	boost::shared_ptr<WebQQ> m_webqq;
};

clean_cache_op make_clean_cache_op(boost::shared_ptr<WebQQ> webqq)
{
	return clean_cache_op(webqq);
}

} // namespace detail


inline void start_clean_cache(boost::shared_ptr<WebQQ> webqq)
{
	detail::make_clean_cache_op(webqq);
}

} // namespace qqimpl
} // namespace webqq
