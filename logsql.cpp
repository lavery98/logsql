#include <znc/Chan.h>
#include <znc/Modules.h>
#include <znc/User.h>

#include <mysql/mysql.h>

class CLogSQL : public CModule {
public:
  MODCONSTRUCTOR(CLogSQL) {}

  ~CLogSQL() override {
    if(mysqlConn)
      mysql_close(mysqlConn);
  }

  void PutLog(const CString& sTarget, const CString& sSender, const CString& sMessage);

  bool OnLoad(const CString& sArgs, CString& sMessage) override;

  EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override;

private:
  CString sHost;
  CString sUserDB;
  CString sPassDB;
  CString sDatabase;
  CString sTable;

  MYSQL *mysqlConn;
};

void CLogSQL::PutLog(const CString& sTarget, const CString& sSender, const CString& sMessage) {
  if(mysqlConn) {
    // Create the query string
    int tableLen = sizeof(char)*strlen(sTable.c_str());
    int targetLen = sizeof(char)*strlen(sTarget.c_str());
    int senderLen = sizeof(char)*strlen(sSender.c_str());
    int messageLen = sizeof(char)*strlen(sMessage.c_str());
    char *target = (char*) malloc(2*targetLen+1);
    char *sender = (char*) malloc(2*senderLen+1);
    char *message = (char*) malloc(2*messageLen+1);
    char *query = (char*) malloc(2*(tableLen+targetLen+senderLen+messageLen)+80);

    mysql_real_escape_string(mysqlConn, target, sTarget.c_str(), targetLen);
    mysql_real_escape_string(mysqlConn, sender, sSender.c_str(), senderLen);
    mysql_real_escape_string(mysqlConn, message, sMessage.c_str(), messageLen);
    sprintf(query, "INSERT INTO %s (`target`, `private`, `sender`, `message`) VALUES ('%s', 0, '%s', '%s')", sTable.c_str(), target, sender, message);
    DEBUG(query);

    // Execute the query
    if(mysql_query(mysqlConn, query)) {
      CString err = mysql_error(mysqlConn);
      DEBUG("MySQL query failed. " + err);
    }

    // Free all variables
    free(target);
    free(sender);
    free(message);
    free(query);
  }
}

bool CLogSQL::OnLoad(const CString& sArgs, CString& sMessage) {
  sHost = sArgs.Token(0);
  sUserDB = sArgs.Token(1);
  sPassDB = sArgs.Token(2);
  sDatabase = sArgs.Token(3);
  sTable = sArgs.Token(4);

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

    mysql_close(mysqlConn);
    return false;
  }

  return true;
}

CModule::EModRet CLogSQL::OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
  PutLog(Channel.GetName(), Nick.GetNick(), sMessage);

  return CONTINUE;
}

template<> void TModInfo<CLogSQL>(CModInfo& Info) {
  Info.SetHasArgs(true);
}

NETWORKMODULEDEFS(CLogSQL, "")
