/*
 * Copyright (C) 2013  mosir, avplayer 开源社区
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

#ifndef __EXCHANGERATE_HPP_
#define __EXCHANGERATE_HPP_

#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>

#include <avhttp.hpp>
#include <avhttp/async_read_body.hpp>

#include "extension.hpp"

#define EOR_Number 75

// exchange of rate 结构体
struct EOR{
  std::string money;
  std::string list;
}  EOR_lists[EOR_Number] = {
		{ "美元指数", "DINIW" },
		{ "美元人民币", "USDCNY" },
		{ "澳元美元", "AUDUSD" },
		{ "欧元美元", "EURUSD" },
		{ "英镑美元", "GBPUSD" },
		{ "新西兰元美元", "NZDUSD" },
		{ "美元加元", "USDCAD" },
		{ "美元瑞郎", "USDCHF" },
		{ "美元港元", "USDHKD" },
		{ "美元日元", "USDJPY" },
		{ "美元马币", "USDMYR" },
		{ "美元新加坡元", "USDSGD" },
		{ "美元台币", "USDTWD" },
		{ "澳元加元", "AUDCAD" },
		{ "澳元瑞郎", "AUDCHF" },
		{ "澳元人民币", "AUDCNY" },
		{ "澳元欧元", "AUDEUR" },
		{ "澳元英镑", "AUDGBP" },
		{ "澳元港元", "AUDHKD" },
		{ "澳元日元", "AUDJPY" },
		{ "澳元新西兰元", "AUDNZD" },
		{ "加元澳元", "CADAUD" },
		{ "加元瑞郎", "CADCHF" },
		{ "加元人民币", "CADCNY" },
		{ "加元欧元", "CADEUR" },
		{ "加元英镑", "CADGBP" },
		{ "加元港元", "CADHKD" },
		{ "加元日元", "CADJPY" },
		{ "加元新西兰元", "CADNZD" },
		{ "瑞郎澳元", "CHFAUD" },
		{ "瑞郎加元", "CHFCAD" },
		{ "瑞郎人民币", "CHFCNY" },
		{ "瑞郎欧元", "CHFEUR" },
		{ "瑞郎英镑", "CHFGBP" },
		{ "瑞郎港元", "CHFHKD" },
		{ "瑞郎日元", "CHFJPY" },
		{ "人民币日元", "CNYJPY" },
		{ "欧元澳元", "EURAUD" },
		{ "欧元加元", "EURCAD" },
		{ "欧元瑞郎", "EURCHF" },
		{ "欧元人民币", "EURCNY" },
		{ "欧元英镑", "EURGBP" },
		{ "欧元港元", "EURHKD" },
		{ "欧元日元", "EURJPY" },
		{ "欧元新西兰元", "EURNZD" },
		{ "英镑澳元", "GBPAUD" },
		{ "英镑加元", "GBPCAD" },
		{ "英镑瑞郎", "GBPCHF" },
		{ "英镑人民币", "GBPCNY" },
		{ "英镑欧元", "GBPEUR" },
		{ "英镑港元", "GBPHKD" },
		{ "英镑日元", "GBPJPY" },
		{ "英镑新西兰元", "GBPNZD" },
		{ "港元澳元", "HKDAUD" },
		{ "港元加元", "HKDCAD" },
		{ "港元瑞郎", "HKDCHF" },
		{ "港元人民币", "HKDCNY" },
		{ "港元欧元", "HKDEUR" },
		{ "港元英镑", "HKDGBP" },
		{ "港元日元", "HKDJPY" },
		{ "新西兰元人民币", "NZDCNY" },
		{ "新加坡元人民币", "SGDCNY" },
		{ "台币人民币", "TWDCNY" },
		{ "美元澳门元", "USDMOP" },
		{ "美元", "USDCNY" },	// !!!单一币种缺省查与人民币的汇率，必须放在最后，避免正则匹配干扰其他项
		{ "澳元", "AUDCNY" },
		{ "加元", "CADCNY" },
		{ "瑞郎", "CHFCNY" },
		{ "欧元", "EURCNY" },
		{ "英镑", "GBPCNY" },
		{ "港元", "HKDCNY" },
		{ "港币", "HKDCNY" },
		{ "新西兰元", "NZDCNY" },
		{ "新加坡元", "SGDCNY" },
		{ "台币", "TWDCNY" }
	};


namespace exchange{

// http://hq.sinajs.cn/?rn=1376405746416&list=USDCNY  
// 只查询sina支持的EOR_lists中的货币汇率
template<class MsgSender>
struct exchangerate_fetcher_op{

	exchangerate_fetcher_op(boost::asio::io_service & _io_service, MsgSender _sender, std::string _money)
	  : io_service(_io_service), sender(_sender), stream(new avhttp::http_stream(_io_service)), money(_money), buf(boost::make_shared<boost::asio::streambuf>())
	{
		std::string list;
		
		for ( int i = 0; i < EOR_Number; i++ )
			if ( money == EOR_lists[i].money ){
				list = EOR_lists[i].list;
				break;
			}
		
		if (list.empty()){
			sender( std::string( money + " 无汇率数据或avbot暂不支持"));
		}else{
			std::string url = boost::str(boost::format("http://hq.sinajs.cn/?_=%d&list=%s") % std::time(0) % list);
			avhttp::async_read_body(*stream, url, * buf, *this);
		}
	}

	void operator()(boost::system::error_code ec, std::size_t bytes_transfered)
	{
		if (!ec || ec == boost::asio::error::eof){
			// 读取 buf 内容, 执行 regex 匹配, 获取汇率.
			std::string str;
			std::string jscript;
			str.resize(bytes_transfered);
			buf->sgetn(&str[0], bytes_transfered);
			jscript = boost::locale::conv::between(boost::trim_copy(str) , "UTF-8", "gbk");

			boost::cmatch what;
			// 2     3     4     5     6     7     8     9     10
			// 当前，卖出，昨收，振幅，今开，最高，最低，买入，名称
			boost::regex ex("var ([^=]*)=\"[0-9]*:[0-9]*:[0-9]*,([0-9\\.\\-]*),([0-9\\.\\-]*),([0-9\\.\\-]*),([0-9\\.\\-]*),([0-9\\.\\-]*),([0-9\\.\\-]*),([0-9\\.\\-]*),([0-9\\.\\-]*),(.*)\"");
			if (boost::regex_search(jscript.c_str(), what, ex))
			{
				std::string msg = boost::str(boost::format("%s 当前汇率 %s 昨收 %s 今开 %s 最高 %s 最低 %S 买入 %s 卖出 %s ")
											% what[10] % what[9] % what[4] % what[6] % what[7] % what[8] % what[9] % what[3]);

				sender(msg);
			}
		}
	}

	boost::shared_ptr<boost::asio::streambuf>  buf;
	boost::asio::io_service & io_service;
	MsgSender sender;
	boost::shared_ptr<avhttp::http_stream> stream;
	std::string money;
};

template<class MsgSender>
void exchangerate_fetcher(boost::asio::io_service & io_service, MsgSender sender, std::string money)
{
	exchangerate_fetcher_op<MsgSender>(io_service, sender, money);
}

}

class exchangerate : avbotextension
{
private:

public:
	template<class MsgSender>
	exchangerate(boost::asio::io_service & _io_service, MsgSender sender, std::string channel_name)
	  : avbotextension(_io_service, sender, channel_name)
	{
	}

	void operator()(const boost::system::error_code& error);

	void operator()(boost::property_tree::ptree msg)
	{
 		if( msg.get<std::string>( "channel" ) != m_channel_name )
			return;

		std::string textmsg = boost::trim_copy( msg.get<std::string>( "message.text" ) );
		
		// 组合正则表达式 str == ".qqbot (美元|欧元|日元|......)(汇率)?"
		std::string str = ".qqbot (";
		for ( int i = 0; i < EOR_Number - 1; i++){
			str += EOR_lists[i].money;
			str += "|";
		}
		str += EOR_lists[ EOR_Number - 1 ].money;
		str += ")(汇率)?";
		
		boost::cmatch what;
		if (boost::regex_search(textmsg.c_str(), what, boost::regex(str)))
		{
			exchange::exchangerate_fetcher(io_service, m_sender, what[1]);
		}
	}
};

#endif // __EXCHANGERATE_HPP_
