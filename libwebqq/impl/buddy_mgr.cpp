
#include "buddy_mgr.hpp"
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/move/move.hpp>

namespace webqq {
namespace qqimpl {

void buddy_mgr::update_group_list(std::string gid, std::string name, std::string code)
{
	using namespace soci;

	std::string qqnum, owner;

	soci::indicator qqnum_indicator, owner_indicator;

	transaction trans(m_sql);

	m_sql << "select qqnum, owner from groups where group_code= :group_code"
		  , into(qqnum, qqnum_indicator), into(owner, owner_indicator), use(code);

	if (qqnum_indicator == i_null)
		qqnum = "";

	if (owner_indicator == i_null)
		owner = "";

	m_sql << "delete from groups where gid = :gid or group_code = :group_code "
		  , use(gid), use(code);

	m_sql << "insert into groups "
		  "(gid, group_code, name, qqnum, owner, generate_time)"
		  " values "
		  "(:gid, :group_code, :name, :qqnum, :owner, datetime('now') )"
		  , use(gid), use(code), use(name), use(qqnum), use(owner);

	trans.commit();
}

bool buddy_mgr::group_has_qqnum(std::string code)
{
	using namespace soci;

	std::string qqnum;
	soci::indicator qqnum_indicator;

	m_sql << "select qqnum from groups where group_code = :group_code"
		, into(qqnum, qqnum_indicator), use(code);

	if (qqnum_indicator == soci::i_ok)
	{
		return  !qqnum.empty();
	}

	return false;
}

void buddy_mgr::map_group_qqnum(std::string code, std::string qqnum)
{
	using namespace soci;

	transaction trans(m_sql);

	m_sql << "update groups set qqnum = :qqnum where group_code = :group_code "
		, use(qqnum), use(code);

	trans.commit();
}

bool buddy_mgr::buddy_has_qqnum(std::string uid)
{
	using namespace soci;

	std::string qqnum;
	soci::indicator qqnum_indicator;

	m_sql << "select qqnum from group_buddies where uid = :uid"
		, into(qqnum, qqnum_indicator), use(uid);

	if (qqnum_indicator == soci::i_ok)
	{
		return  !qqnum.empty();
	}

	return false;
}

void buddy_mgr::map_buddy_qqnum(std::string uid, std::string qqnum)
{
	using namespace soci;

	transaction trans(m_sql);

	m_sql << "update group_buddies set qqnum = :qqnum where uid = :uid "
		, use(qqnum), use(uid);

	trans.commit();
}

qqGroup_ptr buddy_mgr::get_group_by_gid(std::string gid)
{
	qqGroup group;

	soci::indicator indicator;

	group.get_Buddy_by_uin = boost::bind(&buddy_mgr::get_buddy_by_uin, this, _1);
	group.add_new_buddy = boost::bind(&buddy_mgr::group_new_buddy, this, gid, _1, _2, _3);

	m_sql << "select gid, group_code, name, qqnum, owner from groups where gid = :gid"
	  , soci::into(group.gid, indicator)
	  , soci::into(group.code)
	  , soci::into(group.name)
	  , soci::into(group.qqnum)
	  , soci::into(group.owner)
	  , soci::use(gid);

	if (indicator == soci::i_ok)
		return boost::make_shared<qqGroup>(group);
	return qqGroup_ptr();
}

qqGroup_ptr buddy_mgr::get_group_by_qq(std::string qqnum)
{
	std::string gid;

	m_sql << "select gid from groups where qqnum = :qqnum order by generate_time desc"
	  , soci::into(gid)
	  , soci::use(qqnum);

	return get_group_by_gid(gid);
}

std::vector<std::string> buddy_mgr::get_group_all_buddies_uin(std::string gid)
{
	// 一个群最多  500 个,  高级群也不过 2000 个而已. 这里限制一万个, 足够了.
	std::vector<std::string> uins(10000);

	m_sql <<  "select uid from group_buddies where gid = :gid",
		soci::into(uins), soci::use(gid);

	// 依据 C++11 的 move 语义, 这里是非常廉价的.
	// boost.move 在 c++03 里模拟了 move, 允许我们编写可移植程序.
	return boost::move(uins);
}


qqBuddy_ptr buddy_mgr::get_buddy_by_uin(std::string uid)
{
	using namespace soci;

	std::string uin, nick, card, qqnum;
	unsigned int mflag;

	indicator uid_indicator, nick_indicator, card_indicator, mflag_indicator, qqnum_indicator;

	m_sql << "select uid, nick, card, mflag, qqnum from group_buddies where uid=:uid"
		, into(uid, uid_indicator)
		, into(nick, nick_indicator)
		, into(card, card_indicator)
		, into(mflag, mflag_indicator)
		, into(qqnum, qqnum_indicator)
		, use(uid);

	if (uid_indicator!= i_ok)
		return qqBuddy_ptr();

	return boost::make_shared<qqBuddy>(uin, nick, card, mflag, qqnum);
}

void buddy_mgr::set_group_owner(std::string gid, std::string owner)
{
	using namespace soci;

	transaction trans(m_sql);

	m_sql << "update groups set owner = :owner where gid = :gid "
		, use(owner), use(gid);

	trans.commit();
}

void buddy_mgr::group_new_buddy(std::string gid, std::string uid, std::string qqnum, std::string nick)
{
	using namespace soci;

	transaction trans(m_sql);

	m_sql << "insert into group_buddies (gid, uid, qqnum, nick) values (:gid, :uid, :qqnum, :nick)"
		, use(gid), use(uid), use(qqnum), use(nick);

	trans.commit();
}

void buddy_mgr::buddy_update_mflag(std::string uid, unsigned int mflag)
{
	using namespace soci;

	transaction trans(m_sql);

	m_sql << "update group_buddies set mflag = :mflag where uid = :uid "
		, use(uid), use(mflag);

	trans.commit();
}

void buddy_mgr::buddy_update_card(std::string uid, std::string card)
{
	using namespace soci;

	transaction trans(m_sql);

	m_sql << "update group_buddies set card = :card where uid = :uid "
		, use(uid), use(card);

	trans.commit();
}

/**
 * @brief 删除过时的一些信息
 *
 * @return void
 */
void buddy_mgr::clean_out_outdated()
{
	// what is 过时 ?
	// 当然是说时间超过2天的，　以及，失去　gid　衔接的　group_buddies
	soci::transaction transaction(m_sql);

	m_sql << "delete from groups where datetime(generate_time) < datetime('now', '-2 days');";

	m_sql << "delete from group_buddies where gid not in (select gid from groups);";

	transaction.commit();
}

void buddy_mgr::db_initialize()
{
	soci::transaction trans(m_sql);
	// 初始化存储数据库
	m_sql << ""
		  "create table if not exists groups ( "
		  "`gid` TEXT not null,"
		  "`group_code` TEXT not null,"
		  "`name` TEXT not null, "
		  "`qqnum` TEXT, "
		  "`owner` TEXT, "
		  // last time that this group information retrived from TX
		  // libwebqq will remove outdated one
		  "`generate_time` TEXT not null"
		  ");";

	m_sql << ""
		  "create table if not exists group_buddies ( "
		  "`gid` TEXT not null,"
		  "`uid` TEXT not null  UNIQUE ON CONFLICT replace,"
		  "`nick` TEXT,"
		  "`card` TEXT,"
		  "`mflag` INTEGER,"
		  "`qqnum` TEXT"
		  ");";

	trans.commit();
}


} // nsmespace qqimpl
} // namespace webqq
