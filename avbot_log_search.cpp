#include <string>
#include <vector>

#include <boost/log/trivial.hpp>

#include <boost/function.hpp>
#include <boost/property_tree/ptree.hpp>
namespace pt = boost::property_tree;
#include <boost/asio.hpp>

#include <boost/timer/timer.hpp>

#include <soci-sqlite3.h>
#include <boost-optional.h>
#include <boost-tuple.h>
#include <boost-gregorian-date.h>
#include <soci.h>

#include <avhttp/detail/escape_string.hpp>

void avlog_do_search(boost::asio::io_service & io_service,
	std::string c, std::string q, std::string date,
	boost::function<void (boost::system::error_code, pt::ptree)> handler,
	soci::session & db)
{
	pt::ptree outjson;
	std::string q_escaped;
	// 根据 channel_name , query string , date 像数据库查找
	BOOST_LOG_TRIVIAL(debug) << " c = " << c << " q =  " << q << " date= " << date ;

	std::vector<std::string>	r_date(1000);
	std::vector<std::string>	r_channel(1000);
	std::vector<std::string>	r_nick(1000);
	std::vector<std::string>	r_message(1000);

	avhttp::detail::unescape_path(q, q_escaped);

	boost::timer::cpu_timer cputimer;

	cputimer.start();

	db << "select date,channel,nick,message from avlog where channel=:c "
		"and message like \"%" << q_escaped << "%\" order  by strftime(`date`) DESC"
		, soci::into(r_date)
		, soci::into(r_channel)
		, soci::into(r_nick)
		, soci::into(r_message)
		, soci::use(c);

	pt::ptree results;
	// print out the result
	for (int i = 0; i < r_date.size() ; i ++)
	{
		pt::ptree onemsg;
		onemsg.put("date", r_date[i]);
		onemsg.put("channel", r_channel[i]);
		onemsg.put("nick", r_nick[i]);
		onemsg.put("channel", r_channel[i]);
		onemsg.put("message", r_message[i]);

		results.push_back(std::make_pair("", onemsg));
	}

	outjson.put("params.time_used", boost::timer::format(cputimer.elapsed(), 6, "%w"));
	outjson.put("params.num_results", r_date.size());

	outjson.put_child("data", results);

	io_service.post(
		boost::asio::detail::bind_handler(handler,
			boost::system::error_code(),
			outjson
		)
	);
};
