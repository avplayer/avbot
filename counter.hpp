//
//  counter.h
//  qqbot
//
//  Created by hyq on 13-1-15.
//
//

#ifndef __QQBOT__COUNTER__
#define __QQBOT__COUNTER__

#include <iostream>
#include <fstream>
#include <map>

#include <boost/noncopyable.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/tuple/tuple.hpp>


class counter : public boost::noncopyable
{
public:
	counter(std::string filename = "counter.db")
		: filename_(filename)
	{
		load();
	}

	void save()
	{
		std::fstream out(filename_.c_str(), std::fstream::out);

		typedef std::map<
			std::string,
			boost::tuples::tuple<
				std::size_t,
				boost::posix_time::ptime
			>
		>::iterator iterator;

		for (iterator iter = map_.begin(); iter != map_.end(); iter++)
		{
			out << iter->first << "\t" << boost::get<0>(iter->second)
				<< "\t" << boost::get<1>(iter->second) << "\n";
		}
	}

	void increace(std::string qq)
	{
		boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		boost::get<0>(map_[qq]) ++ ;
		boost::get<1>(map_[qq]) = now;
	}

private:
	std::string filename_;
	std::map<
		std::string,
		boost::tuples::tuple<
			std::size_t, boost::posix_time::ptime
		>
	> map_;

	void load()
	{
		std::fstream in(filename_.c_str(), std::ios_base::in);
		std::string key;
		std::size_t count;
		boost::posix_time::ptime time;

		if (in.good())
		{
			while (in >> key >> count >> time)
			{
				boost::get<0>(map_[key]) = count;
				boost::get<1>(map_[key]) = time;
			}
		}
	}

};

#endif /* defined(__QQBOT__COUNTER__) */
