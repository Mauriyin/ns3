#pragma once
#include "ns3/global-value.h"
#include "ns3/ns3-ai-rl.h"
#include "wifi-phy.h"

namespace ns3 {
struct DcfRlEnv
{
  int64_t time;
  double power;
  uint32_t idx;
};
struct DcfRlAct
{
  bool ccaBusy;
};

class DcfRl : public Ns3AIRL<DcfRlEnv, DcfRlAct>
{
  DcfRl (void) = delete;

public:
  DcfRl (uint16_t id);
  bool step (Ptr<WifiPhy> phy);
};

extern Ptr<DcfRl> gDcfRl;
extern GlobalValue gUseRl;
} // namespace ns3