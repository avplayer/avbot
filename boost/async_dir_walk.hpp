#pragma once

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include <boost/async_foreach.hpp>

namespace boost{
namespace detail{

class DirWalkDumyHandler{
public:
	void operator()(boost::system::error_code ec){}
};

} // namespace detail

typedef function<void(boost::system::error_code ec) > async_dir_walk_continue_handler;

template<class DirWalkHandler, class CompleteHandler>
void async_dir_walk(boost::asio::io_service & io_service, boost::filesystem::path path, DirWalkHandler dir_walk_handler, CompleteHandler complete_handler)
{
	async_foreach(io_service, boost::filesystem::directory_iterator(path), boost::filesystem::directory_iterator(), dir_walk_handler, complete_handler);
}

template<class DirWalkHandler>
void async_dir_walk(boost::asio::io_service & io_service, boost::filesystem::path path, DirWalkHandler dir_walk_handler)
{
	async_dir_walk(io_service, path,  dir_walk_handler, detail::DirWalkDumyHandler());
}

}
