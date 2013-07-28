
#include "error_code.hpp"

const boost::system::error_category& webqq::error::error_category()
{
	static error_category_impl error_category_instance;
	return reinterpret_cast<const boost::system::error_category&>(error_category_instance);
}

const char* webqq::error::error_category_impl::name() const BOOST_SYSTEM_NOEXCEPT
{
	return "libwebqq";
}

std::string webqq::error::error_category_impl::message(int e) const
{
	switch(e)
	{
		case ok:
			return "Sucess";

		case login_check_need_vc:
			return "Need Vercode";

		case login_failed_server_busy:
			return "Server busy! Please try again";
		case login_failed_qq_outdate:
			return "QQ number outdate and disabled";
		case login_failed_wrong_passwd:
			return "wrong password";
		case login_failed_wrong_vc:
			return "wrong vc code";
		case login_failed_verify_failed:
			return "!!!!!!!!!! Verify failed !!!!!!!!";
		case login_failed_try_again:
			return "You may need to try login again !";
		case login_failed_wrong_input:
			return "Wrong input";
		case login_failed_too_many_login:
			return "too many login on this ip";
		case login_failed_other:
			return "Login failed unknow reason";

		case fetch_verifycode_failed:
			return "cannot fetch verifycode";
		case failed_to_change_status:
			return "cannot change status";
		case poll_failed_network_error:
			return "failed to poll message, network error";
		case failed_to_fetch_group_list:
			return "cannot fetch group list";
		case failed_to_fetch_group_qqnumber:
			return "cannot get group qq number";
		case poll_failed_need_login:
			return "user not login";
		case poll_failed_user_kicked_off:
			return "user kicked off by system";
		default:
			return "Unknown error";
	}
}
