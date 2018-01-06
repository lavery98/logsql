#include <znc/Modules.h>

class CLogSQL : public CModule {
public:
  MODCONSTRUCTOR(CLogSQL) {}

  ~CLogSQL() override {}
};

template<> void TModInfo<CLogSQL>(CModInfo& Info) {
  Info.SetHasArgs(true);
}

NETWORKMODULEDEFS(CLogSQL, "")
