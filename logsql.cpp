#include <znc/Chan.h>
#include <znc/Modules.h>
#include <znc/User.h>

#include <mysql/mysql.h>

using std::map;
using std::vector;

class CLogSQL : public CModule {
public:
  MODCONSTRUCTOR(CLogSQL) {}

  ~CLogSQL() override {
    if(mysqlConn)
      mysql_close(mysqlConn);
  }

  void PutLog(const CString& sTarget, const CString& sSender, const CString& sType, const CString& sMessage, const int priv = 0);
  void PutLog(const CChan& Channel, const CString& sSender, const CString& sType, const CString& sMessage);

  bool OnLoad(const CString& sArgs, CString& sMessage) override;
  void OnIRCConnected() override;
  void OnIRCDisconnected() override;

  void OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs) override;
  void OnKick(const CNick& Nick, const CString& sOpNick, CChan& Channel, const CString& sMessage) override;
  void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) override;
  void OnJoin(const CNick& Nick, CChan& Channel) override;
  void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) override;
  void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) override;
  EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override;

  EModRet OnUserNotice(CString& sTarget, CString& sMessage) override;
  EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override;
  EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override;

  EModRet OnUserAction(CString& sTarget, CString& sMessage) override;
  EModRet OnPrivAction(CNick& Nick, CString& sMessage) override;
  EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) override;

  EModRet OnUserMsg(CString& sTarget, CString& sMessage) override;
  EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override;
  EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override;

private:
  CString sHost;
  CString sUserDB;
  CString sPassDB;
  CString sDatabase;
  CString sTable;

  MYSQL *mysqlConn;
};

void CLogSQL::PutLog(const CString& sTarget, const CString& sSender, const CString& sType, const CString& sMessage, const int priv) {
  if(mysqlConn) {
    // Create the query string
    int tableLen = sizeof(char)*strlen(sTable.c_str());
    int targetLen = sizeof(char)*strlen(sTarget.c_str());
    int senderLen = sizeof(char)*strlen(sSender.c_str());
    int typeLen = sizeof(char)*strlen(sType.c_str());
    int messageLen = sizeof(char)*strlen(sMessage.c_str());
    char *target = (char*) malloc(2*targetLen+1);
    char *sender = (char*) malloc(2*senderLen+1);
    char *message = (char*) malloc(2*messageLen+1);
    char *query = (char*) malloc(2*(tableLen+targetLen+senderLen+typeLen+messageLen)+(12+61+4+1+4+4+4+2)+1);

    mysql_real_escape_string(mysqlConn, target, sTarget.c_str(), targetLen);
    mysql_real_escape_string(mysqlConn, sender, sSender.c_str(), senderLen);
    mysql_real_escape_string(mysqlConn, message, sMessage.c_str(), messageLen);
    sprintf(query, "INSERT INTO %s (`target`, `private`, `sender`, `type`, `message`) VALUES ('%s', '%d', '%s', '%s', '%s')", sTable.c_str(), target, priv, sender, sType.c_str(), message);
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

void CLogSQL::PutLog(const CChan& Channel, const CString& sSender, const CString& sType, const CString& sMessage) {
  const map<unsigned char, CString>& modes = Channel.GetModes();
  int priv = 0;

  for(const auto& it : modes) {
    if(it.first == 's' || it.first == 'p') {
      priv = 1;

      break;
    }
  }

  PutLog(Channel.GetName(), sSender, sType, sMessage, priv);
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

void CLogSQL::OnIRCConnected() {
}

void CLogSQL::OnIRCDisconnected() {
}

void CLogSQL::OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
}

void CLogSQL::OnKick(const CNick& Nick, const CString& sOpNick, CChan& Channel, const CString& sMessage) {
}

void CLogSQL::OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
}

void CLogSQL::OnJoin(const CNick& Nick, CChan& Channel) {
  PutLog(Channel, Nick.GetNick(), "JOIN", "");
}

void CLogSQL::OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
  PutLog(Channel, Nick.GetNick(), "PART", sMessage);
}

void CLogSQL::OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) {
  for(CChan* pChan : vChans) {
    PutLog(*pChan, Nick.GetNick(), "NICK", sNewNick);
  }
}

CModule::EModRet CLogSQL::OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) {
  PutLog(Channel, Nick.GetNick(), "TOPIC", sTopic);

  return CONTINUE;
}

CModule::EModRet CLogSQL::OnUserNotice(CString& sTarget, CString& sMessage) {
  return CONTINUE;
}

CModule::EModRet CLogSQL::OnPrivNotice(CNick& Nick, CString& sMessage) {
  return CONTINUE;
}

CModule::EModRet CLogSQL::OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) {
  PutLog(Channel, Nick.GetNick(), "NOTICE", sMessage);

  return CONTINUE;
}

CModule::EModRet CLogSQL::OnUserAction(CString& sTarget, CString& sMessage) {
  return CONTINUE;
}

CModule::EModRet CLogSQL::OnPrivAction(CNick& Nick, CString& sMessage) {
  return CONTINUE;
}

CModule::EModRet CLogSQL::OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) {
  return CONTINUE;
}

CModule::EModRet CLogSQL::OnUserMsg(CString& sTarget, CString& sMessage) {
  return CONTINUE;
}

CModule::EModRet CLogSQL::OnPrivMsg(CNick& Nick, CString& sMessage) {
  return CONTINUE;
}

CModule::EModRet CLogSQL::OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
  PutLog(Channel, Nick.GetNick(), "PRIVMSG", sMessage);

  return CONTINUE;
}

template<> void TModInfo<CLogSQL>(CModInfo& Info) {
  Info.SetHasArgs(true);
}

NETWORKMODULEDEFS(CLogSQL, "")
