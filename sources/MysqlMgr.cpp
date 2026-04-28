#include "../headers/MysqlMgr.h"

std::optional<uint64_t> MysqlMgr::registerUser(const std::string& user, const std::string& password, const std::string& email)
{
	return _dao.registerUser(user, password, email);
}

bool MysqlMgr::checkEmail(const std::string& user, const std::string& email)
{
	return _dao.checkEmail(user, email);
}

bool MysqlMgr::checkPwd(const std::string& user, const std::string& password, UserInfo& userInfo)
{
	return _dao.checkPwd(user, password, userInfo);
}

bool MysqlMgr::updatePassword(const std::string& email, const std::string& password)
{
	return _dao.updatePassword(email, password);
}
