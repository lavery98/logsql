#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
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

  void PutLog(const CString& sSource, const CString& sCommand, const CString& sTarget, const CString& sMessage, const int priv = 0);
  void PutLog(const CString& sSource, const CString& sCommand, const CChan& Channel, const CString& sMessage);

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

void CLogSQL::PutLog(const CString& sSource, const CString& sCommand, const CString& sTarget, const CString& sMessage, const int priv) {
  if(mysqlConn) {
    // Create the query string
    int tableLen = sizeof(char)*strlen(sTable.c_str());
    int targetLen = sizeof(char)*strlen(sTarget.c_str());
    int sourceLen = sizeof(char)*strlen(sSource.c_str());
    int commandLen = sizeof(char)*strlen(sCommand.c_str());
    int messageLen = sizeof(char)*strlen(sMessage.c_str());
    char *target = (char*) malloc(2*targetLen+1);
    char *source = (char*) malloc(2*sourceLen+1);
    char *message = (char*) malloc(2*messageLen+1);
    //char *query = (char*) malloc(2*(tableLen+targetLen+senderLen+typeLen+messageLen)+(12+61+4+1+4+4+4+2)+1);
    char *query = (char*) malloc(2*(tableLen+targetLen+sourceLen+commandLen+messageLen)+(12+53+4+4+4+2)+1);

    mysql_real_escape_string(mysqlConn, target, sTarget.c_str(), targetLen);
    mysql_real_escape_string(mysqlConn, source, sSource.c_str(), sourceLen);
    mysql_real_escape_string(mysqlConn, message, sMessage.c_str(), messageLen);
    //sprintf(query, "INSERT INTO %s (`target`, `private`, `sender`, `type`, `message`) VALUES ('%s', '%d', '%s', '%s', '%s')", sTable.c_str(), target, priv, sender, sType.c_str(), message);
    sprintf(query, "INSERT INTO %s (`source`, `command`, `target`, `message`) VALUES ('%s', '%s', '%s', '%s')", sTable.c_str(), source, sCommand.c_str(), target, message);
    DEBUG(query);

    // Execute the query
    if(mysql_query(mysqlConn, query)) {
      CString err = mysql_error(mysqlConn);
      DEBUG("MySQL query failed. " + err);
    }

    // Free all variables
    free(target);
    free(source);
    free(message);
    free(query);
  }
}

void CLogSQL::PutLog(const CString& sSource, const CString& sCommand, const CChan& Channel, const CString& sMessage) {
  const map<unsigned char, CString>& modes = Channel.GetModes();
  int priv = 0;

  for(const auto& it : modes) {
    if(it.first == 's' || it.first == 'p') {
      priv = 1;

      break;
    }
  }

  PutLog(sSource, sCommand, Channel.GetName(), sMessage, priv);
}

bool CLogSQL::OnLoad(const CString& sArgs, CString& sMessage) {
  // TODO: use a seperate page to manage the database settings

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

  // Log channel modes on load
  CIRCNetwork* pNetwork = GetNetwork();
  vector<CChan*> vChans = pNetwork->GetChans();

  for(CChan* pChan : vChans) {
    //PutLog(*pChan, "", "LOAD", pChan->GetModeString());
  }

  return true;
}

void CLogSQL::OnIRCConnected() {
}

void CLogSQL::OnIRCDisconnected() {
}

void CLogSQL::OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
  CString sNick = pOpNick ? pOpNick->GetNick() : "Server";
  PutLog(sNick, "MODE", Channel, sModes + " " + sArgs);
}

void CLogSQL::OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
  //PutLog(Channel, OpNick.GetNick(), "KICK", " kicked " + sKickedNick + " (" + sMessage + ")");
}

void CLogSQL::OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
  for(CChan* pChan : vChans) {
    PutLog(Nick.GetNick(), "QUIT", *pChan, sMessage);
  }
}

void CLogSQL::OnJoin(const CNick& Nick, CChan& Channel) {
  PutLog(Nick.GetNick(), "JOIN", Channel, "");
}

void CLogSQL::OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
  PutLog(Nick.GetNick(), "PART", Channel, sMessage);
}

void CLogSQL::OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans) {
  for(CChan* pChan : vChans) {
    PutLog(Nick.GetNick(), "NICK", *pChan, sNewNick);
  }
}

CModule::EModRet CLogSQL::OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) {
  PutLog(Nick.GetNick(), "TOPIC", Channel, sTopic);

  return CONTINUE;
}

CModule::EModRet CLogSQL::OnUserNotice(CString& sTarget, CString& sMessage) {
  return CONTINUE;
}

CModule::EModRet CLogSQL::OnPrivNotice(CNick& Nick, CString& sMessage) {
  return CONTINUE;
}

CModule::EModRet CLogSQL::OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) {
  PutLog(Nick.GetNick(), "NOTICE", Channel, sMessage);

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
  CIRCNetwork* pNetwork = GetNetwork();
  if(pNetwork) {
    CChan* pChannel = pNetwork->FindChan(sTarget);
    if(pChannel) {
      PutLog(pNetwork->GetCurNick(), "PRIVMSG", *pChannel, sMessage);
    } else {
      PutLog(pNetwork->GetCurNick(), "PRIVMSG", sTarget, sMessage);
    }
  }

  return CONTINUE;
}

CModule::EModRet CLogSQL::OnPrivMsg(CNick& Nick, CString& sMessage) {
  CIRCNetwork* pNetwork = GetNetwork();
  if(pNetwork) {
    PutLog(Nick.GetNick(), "PRIVMSG", pNetwork->GetCurNick(), sMessage);
  }

  return CONTINUE;
}

CModule::EModRet CLogSQL::OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
  PutLog(Nick.GetNick(), "PRIVMSG", Channel, sMessage);

  return CONTINUE;
}

template<> void TModInfo<CLogSQL>(CModInfo& Info) {
  Info.SetHasArgs(true);
}

NETWORKMODULEDEFS(CLogSQL, "")
