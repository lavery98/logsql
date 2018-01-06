#include <znc/Modules.h>

#include <mysql/mysql.h>

class CLogSQL : public CModule {
public:
  MODCONSTRUCTOR(CLogSQL) {}

  ~CLogSQL() override {
    if(mysqlConn)
      mysql_close(mysqlConn);
  }

  bool OnLoad(const CString& sArgs, CString& sMessage) override;

  EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override;

private:
  CString sHost;
  CString sUserDB;
  CString sPassDB;
  CString sDatabase;

  MYSQL *mysqlConn;
};

bool CLogSQL::OnLoad(const CString& sArgs, CString& sMessage) {
  sHost = sArgs.Token(0);
  sUserDB = sArgs.Token(1);
  sPassDB = sArgs.Token(2);
  sDatabase = sArgs.Token(3);

  mysqlConn = mysql_init(NULL);

  if(!mysqlConn) {
    sMessage = "Could not initiate a MySQL connection";

    DEBUG("mysql_init() failed.");

    return false;
  }

  my_bool reconnect = 1;
  mysql_options(mysqlConn, MYSQL_OPT_RECONNECT, &reconnect);

  if(!mysql_real_connect(mysqlConn, sHost.c_str(), sUserDB.c_str(), sPassDB.c_str(), sDatabase.c_str(), 0, NULL, 0)) {
    sMessage = "Could not connect to the MySQL server";

    CString err = mysql_error(mysqlConn);
    DEBUG("Could not connect to MySQL server. " + err);

    return false;
  }

  return true;
}

CModule::EModRet CLogSQL::OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
  return CONTINUE;
}

template<> void TModInfo<CLogSQL>(CModInfo& Info) {
  Info.SetHasArgs(true);
}

NETWORKMODULEDEFS(CLogSQL, "")
