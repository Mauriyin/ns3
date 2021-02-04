#include "dcf-rl-env.h"
#include "ns3/boolean.h"

namespace ns3 {
GlobalValue gUseRl = GlobalValue ("UseRl", "UseRl", BooleanValue (false), MakeBooleanChecker ());
DcfRl::DcfRl (uint16_t id) : Ns3AIRL<DcfRlEnv, DcfRlAct> (id)
{
  SetCond (2, 0);
}

bool
DcfRl::step (Ptr<WifiPhy> phy)
{
  auto env = EnvSetterCond ();
  // for (uint32_t i = 0; i < m_phys.size (); i++)
  //   {
  //     if (m_phys[i])
  //       {
  //         env->flag[i] = true;
  //         env->power[i] = m_phys[i]->m_interference.m_firstPower;
  //       }
  //   }
  env->time = Simulator::Now ().GetMicroSeconds ();
//   env->power = phy->m_interference.m_firstPower;
  env->idx = phy->m_pid;
  SetCompleted ();
  auto act = ActionGetterCond ();
  // for (uint32_t i = 0; i < m_phys.size (); i++)
  //   {
  //     NS_UNUSED (act->ccaBusy[i]);
  //   }
  bool ret = act->ccaBusy;
  GetCompleted ();
  return ret;
}

Ptr<DcfRl> gDcfRl = Create<DcfRl> (1357);
} // namespace ns3