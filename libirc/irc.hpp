/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2012 InvXp <invidentssc@hotmail.com>
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
 * along with shared_from_this() program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
RFC Protocol
http://www.irchelp.org/irchelp/rfc/rfc.html
*/

#pragma once

#include <string>
#include <boost/function.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/shared_ptr.hpp>

namespace irc {

struct irc_msg
{
	std::string whom;
	std::string from;
	std::string locate;
	std::string msg;
};

typedef boost::function<void(irc_msg msg)> privmsg_cb;

namespace  impl {
class client_impl;
}

class client
{
public:
	client(boost::asio::io_service &_io_service, const std::string& user,
		   const std::string& user_pwd = "", const std::string& server = "irc.freenode.net",
		   const unsigned int max_retry_count = 1000);

	~client();

	void on_privmsg_message(const privmsg_cb &cb);

	void join(const std::string& ch, const std::string &pwd = "");

	void chat(const std::string whom, const std::string msg);
private:
	boost::shared_ptr<impl::client_impl> impl;
};

}
