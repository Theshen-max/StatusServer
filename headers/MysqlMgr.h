#ifndef MYSQLMGR_H
#define MYSQLMGR_H

#include "Singleton.h"
#include "MysqlDao.h"

class MysqlMgr: public Singleton<MysqlMgr>
{
	friend class Singleton<MysqlMgr>;
public:
	std::optional<uint64_t> registerUser(const std::string& user, const std::string& password, const std::string& email);

	bool checkEmail(const std::string& user, const std::string& email);

	bool checkPwd(const std::string& user, const std::string& password, UserInfo& userInfo);

	bool updatePassword(const std::string& email, const std::string& password);

private:
	MysqlMgr() = default;

	MysqlDao _dao;
};

#endif //MYSQLMGR_H
