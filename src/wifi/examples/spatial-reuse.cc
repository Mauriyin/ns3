/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
//
//  This example program can be used to experiment with spatial
//  reuse mechanisms of 802.11ax.
//
//  The geometry is as follows:
//
//  There are two APs (AP1 and AP2) separated by distance d.
//  Around each AP, there are n STAs dropped uniformly in a circle of
//  radious r (i.e., using hte UnitDiscPositionAllocator)..
//  Parameters d, n, and r are configurable command line arguments
//
//  Each AP and STA have configurable traffic loads (traffic generators).
//  A simple Friis path loss model is used.
//
//  Key confirmation parameters available through command line options, include:
//  --duration             Duration of simulation, in seconds
//  --powSta               Power of STA (dBm)
//  --powAp                Power of AP (dBm)
//  --ccaTrSta             CCA threshold of STA (dBm)
//  --ccaTrAp              CCA threshold of AP (dBm)
//  --d                    Distance between AP1 and AP2, in meters
//  --n                    Number of STAs to scatter around each AP
//  --r                    Radius of circle around each AP in which to scatter STAS, in meters
//  --uplink               Total aggregate uplink load, STAs->AP (Mbps).  Allocated pro rata to each link.
//  --downlink             Total aggregate downlink load,  AP->STAs (Mbps).  Allocated pro rata to each link.
//  --standard             802.11 standard.  E.g., "11ax_5GHZ"
//  --bw                   Bandwidth (consistent with standard), in MHz
//  --enableObssPd         Enable OBSS_PD.  Default is True for 11ax only, false for others
//
//  The basic data to be collected is:
//
//    - Average RSSI (signal strength of beacons)
//    - CCAT (CS/CCA threshold) in dBm
//    - raw signal strength data on all STAs/APs
//    - throughput and fairness
//    - NAV timer traces (and in general, the carrier sense state machine)
//
//  The attributes to be controlled are
//    - OBSS_PD threshold (dB)
//    - Tx power (W or dBm)
//    - DSC Margin (dBs)
//    - DSC Upper Limit (dBs)
//    - DSC Implemented (boolean true or false)
//    - Beacon Count Limit (limit of consecutive missed beacons)
//    - RSSI Decrement (dB) (if Beacon Count Limit exceeded)
//    - use of RTS/CTS
//
//  Some key findings to investigate:
//    - Tanguy Ropitault reports that margin highly affects performance,
//      that DSC margin of 25 dB gives best results, and DSC can increase
//      aggregate throughput compared to Legacy by 80%
//      Ref:  11-16-0604-02-00ax (0604r1), May 2016
//
//  In general, the program can be configured at run-time by passing
//  command-line arguments.  The command
//  ./waf --run "spatial-reuse --help"
//  will display all of the available run-time help options.

#include <iostream>
#include <iomanip>
#include <ns3/core-module.h>
#include <ns3/config-store-module.h>
#include <ns3/network-module.h>
#include <ns3/mobility-module.h>
#include <ns3/internet-module.h>
#include <ns3/wifi-module.h>
#include <ns3/spectrum-module.h>
#include <ns3/applications-module.h>
#include <ns3/propagation-module.h>
//#include <ns3/ieee-80211ax-indoor-propagation-loss-model.h>
//#include <ns3/itu-umi-propagation-loss-model.h>
#include <ns3/flow-monitor-module.h>
#include <ns3/flow-monitor-helper.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpatialReuse");

struct SignalArrival
{
  Time m_time;
  Time m_duration;
  bool m_wifi;
  uint32_t m_nodeId;
  uint32_t m_senderNodeId;
  double m_power;
};

// Global variables for use in callbacks.
double g_signalDbmAvg;
double g_noiseDbmAvg;
uint32_t g_samples;
std::ofstream g_rxSniffFile;
std::ofstream g_txPowerFile;
std::ofstream g_phyResetFile;

double g_min_signal = 1000.0;
double g_max_signal = -1000.0;
double g_min_noise = 1000.0;
double g_max_noise = -1000.0;

uint32_t beaconInterval = 102400; // microseconds
double duration = 20.0; // seconds
uint32_t nBss = 2; // number of BSSs.  Can be 1 or 2 (default)
uint32_t n = 1; // number of STAs to scatter around each AP;
double aggregateUplinkMbps = 1.0;
double aggregateDownlinkMbps = 1.0;

uint32_t nEstablishedAddaBa = 0;
bool allAddBaEstablished = false;
Time timeAllAddBaEstablished;
Time timeLastPacketReceived;
bool filterOutNonAddbaEstablished = false;

uint32_t nAssociatedStas = 0;
bool allStasAssociated = false;
std::vector<uint32_t> nAssociatedStasPerBss (0);

std::vector<Time> busyTime;

NetDeviceContainer apDeviceA, staDevicesA; 
NetDeviceContainer apDeviceB, staDevicesB;
NetDeviceContainer apDeviceC, staDevicesC;
NetDeviceContainer apDeviceD, staDevicesD;
NetDeviceContainer apDeviceE, staDevicesE;
NetDeviceContainer apDeviceF, staDevicesF; 
NetDeviceContainer apDeviceG, staDevicesG;
 

// for tracking packets and bytes received. will be reallocated once we finalize number of nodes
std::vector<uint64_t> packetsReceived (0);
std::vector<uint64_t> bytesReceived (0);
std::vector<uint64_t> bytesTransmitted (0);

std::vector<std::vector<uint64_t> > packetsReceivedPerNode;
std::vector<std::vector<double> > rssiPerNode;

ApplicationContainer uplinkServerApps;
ApplicationContainer downlinkServerApps;
ApplicationContainer uplinkClientApps;
ApplicationContainer downlinkClientApps;

NodeContainer allNodes;

bool verifyResults = false; //for regression
double minExpectedThroughputBss1Up = 0; //for regression
double maxExpectedThroughputBss1Up = 0; //for regression
double minExpectedThroughputBss1Down = 0; //for regression
double maxExpectedThroughputBss1Down = 0; //for regression

// Parse context strings of the form "/NodeList/3/DeviceList/1/Mac/Assoc"
// to extract the NodeId
uint32_t
ContextToNodeId (std::string context)
{
  std::string sub = context.substr (10);  // skip "/NodeList/"
  uint32_t pos = sub.find ("/Device");
  uint32_t nodeId = atoi (sub.substr (0, pos).c_str ());
  NS_LOG_DEBUG ("Found NodeId " << nodeId);
  return nodeId;
}

// Find nodeId given a MacAddress
int
MacAddressToNodeId (Mac48Address macAddress)
{
  int nodeId = -1;
  Mac48Address inAddress = macAddress;
  uint32_t nNodes = allNodes.GetN ();
  for (uint32_t n = 0; n < nNodes; n++)
    {
      Mac48Address nodeAddress = Mac48Address::ConvertFrom (allNodes.Get (n)->GetDevice (0)->GetAddress ());
      if (inAddress == nodeAddress)
        {
          nodeId = n;
          break;
        }
    }
  return nodeId;
}

void
PacketTx (std::string context, Ptr<const Packet> packet, double txPowerW)
{
  if (packet)
    {
      WifiMacHeader hdr;
      packet->PeekHeader (hdr);
      Mac48Address addr1 = hdr.GetAddr1 ();
      Mac48Address addr2 = hdr.GetAddr2 ();
      Mac48Address whatsThis = Mac48Address ("00:00:00:00:00:00");
      if (!addr1.IsBroadcast () && (addr2 != whatsThis))
        {
          int dstNodeId = MacAddressToNodeId (addr1);
          int srcNodeId = MacAddressToNodeId (addr2);
          g_txPowerFile << srcNodeId << ", " << dstNodeId << ", " << WToDbm (txPowerW) << std::endl;
        }
    }
}
uint32_t
Ipv4AddressToNodeId (Ipv4Address address)
{
std::stringstream address_str;
address.Print(address_str);
std::string sub = address_str.str().substr (10);
uint32_t nodeId = atoi (sub.c_str ());
if ((nodeId%(n+1))==0)
{
nodeId=(nodeId/(n+1)-1)*(n+1);
}
  return nodeId;
}
void
PacketRx (std::string context, const Ptr<const Packet> p, const Address &srcAddress, const Address &destAddress)
{
  uint32_t nodeId = ContextToNodeId (context);
  uint32_t pktSize = p->GetSize ();
  if (!filterOutNonAddbaEstablished || allAddBaEstablished)
    {
      bytesReceived[nodeId] += pktSize;
      packetsReceived[nodeId]++;
    }
  timeLastPacketReceived = Simulator::Now();

uint32_t nodeId2= Ipv4AddressToNodeId(InetSocketAddress::ConvertFrom (srcAddress).GetIpv4 ());
  if (!filterOutNonAddbaEstablished || allAddBaEstablished)
    {
      bytesTransmitted[nodeId2] += pktSize;
    }

}

void
PhyReset (std::string context, uint8_t bssColor, double rssiDbm, bool powerRestricted, double txPowerMaxDbmSiso, double txPowerMaxDbmMimo)
{
  uint32_t nodeId = ContextToNodeId (context);
  g_phyResetFile << Simulator::Now () << ", " << nodeId<< ", " << +bssColor<< ", " << rssiDbm<< ", " << txPowerMaxDbmSiso<< ", " << txPowerMaxDbmMimo << std::endl;
}

// loggine arrivals to phy-log.dat file is
// bloating memory duration simulation, and
// those results are not used for any post-processing
// so setting flag here to disable their capture
bool g_logArrivals = false;
std::vector<SignalArrival> g_arrivals;
double g_arrivalsDurationCounter = 0;
std::ofstream g_stateFile;
std::ofstream g_TGaxCalibrationTimingsFile;
std::string
StateToString (WifiPhyState state)
{
  std::string stateString;
  switch (state)
    {
    case WifiPhyState::IDLE:
      stateString = "IDLE";
      break;
    case WifiPhyState::CCA_BUSY:
      stateString = "CCA_BUSY";
      break;
    case WifiPhyState::TX:
      stateString = "TX";
      break;
    case WifiPhyState::RX:
      stateString = "RX";
      break;
    case WifiPhyState::SWITCHING:
      stateString = "SWITCHING";
      break;
    case WifiPhyState::SLEEP:
      stateString = "SLEEP";
      break;
    case WifiPhyState::OFF:
      stateString = "OFF";
      break;
    default:
      NS_FATAL_ERROR ("Unknown state");
    }
  return stateString;
}

Time lastTxAmpduEnd = Seconds (0);
Time lastRxBlockAckEnd = Seconds (0);
Time lastAmpduDuration = Seconds (0);
Time lastBlockAckDuration = Seconds (-1);
Time lastDeferAndBackoffDuration = Seconds (-1);
Time lastSifsDuration = Seconds (-1);

void WriteCalibrationResult (std::string context, std::string type, Time duration,  Ptr<const Packet> p, const WifiMacHeader &hdr)
{
  Time t_now = Simulator::Now ();
  g_TGaxCalibrationTimingsFile << "---------" << std::endl;
  g_TGaxCalibrationTimingsFile << *p << " " << hdr << std::endl;
  g_TGaxCalibrationTimingsFile << t_now << " " << context << " " << type << " " << duration << std::endl;
}

void TxAmpduCallback (std::string context, Ptr<const Packet> p, const WifiMacHeader &hdr)
{
  Time t_now = Simulator::Now ();
  Time t_duration = hdr.GetDuration ();

  // TGax calibration checkpoint calculations
  Time t_cp1 = t_now;
  Time t_cp2 = t_now + t_duration;

  Time ampduDuration = t_cp2 - t_cp1;  // same as t_duration

  WriteCalibrationResult (context, "A-MPDU-duration", ampduDuration, p, hdr);
  lastAmpduDuration = ampduDuration;

  lastTxAmpduEnd = t_cp2;

  if (lastRxBlockAckEnd > Seconds (0))
    {
      Time t_cp5 = t_now;
      Time t_cp4 = lastRxBlockAckEnd;

      Time deferAndBackoffDuration = t_cp5 - t_cp4;
      WriteCalibrationResult (context, "Defer-and-backoff-duration", deferAndBackoffDuration, p, hdr);
      lastDeferAndBackoffDuration = deferAndBackoffDuration;
    }
}

void RxBlockAckCallback (std::string context, Ptr<const Packet> p, const WifiMacHeader &hdr)
{
  Time t_now = Simulator::Now ();
  Time t_duration = hdr.GetDuration ();

  // TGax calibration checkpoint calculations
  Time t_cp3 = t_now;
  Time t_cp4 = t_now + t_duration;

  Time sifsDuration = t_cp3 - lastTxAmpduEnd;
  WriteCalibrationResult (context, "Sifs-duration", sifsDuration, p, hdr);
  lastSifsDuration = sifsDuration;

  Time blockAckDuration = t_cp4 - t_cp3;  // same as t_duration
  WriteCalibrationResult (context, "Block-ACK-duration", blockAckDuration, p, hdr);
  lastBlockAckDuration = blockAckDuration;

  lastRxBlockAckEnd = t_cp4;

}

void
StateCb (std::string context, Time start, Time duration, WifiPhyState state)
{
  uint32_t nodeId = ContextToNodeId (context);
  uint32_t bss = 1;
  bool isAp = false;
  for (; bss <= nBss; bss++)
    {
      if (nodeId == (((bss - 1) * n) + bss - 1))
      {
        isAp = true;
      }
      if (nodeId <= (bss * n) + bss - 1)
        {
          break;
        }
    }
  if ((!filterOutNonAddbaEstablished || allAddBaEstablished) && isAp && ((state == WifiPhyState::TX) || (state == WifiPhyState::RX)))
    {
      busyTime[bss - 1] += duration;
    }
  g_stateFile << ContextToNodeId (context) << " " << start.GetSeconds () << " " << duration.GetSeconds () << " " << StateToString (state) << std::endl;
}

void
AddbaStateCb (std::string context, Time t, Mac48Address recipient, uint8_t tid, OriginatorBlockAckAgreement::State state)
{
  bool isAp = false;
  for (uint32_t bss = 1; bss <= nBss; bss++)
  {
    if (ContextToNodeId (context) == (((bss - 1) * n) + bss - 1))
    {
      isAp = true;
    }
  }
  if (state == OriginatorBlockAckAgreement::ESTABLISHED)
    {
      if ((aggregateDownlinkMbps != 0) && (aggregateUplinkMbps != 0)) //UP + DL
        {
          nEstablishedAddaBa++;
          if (nEstablishedAddaBa == 2 * n * nBss)
            {
              allAddBaEstablished = true;
              timeAllAddBaEstablished = t;
              if (filterOutNonAddbaEstablished)
                {
                  Simulator::Stop (Seconds (duration));
                  for (auto it = uplinkClientApps.Begin (); it != uplinkClientApps.End (); ++it)
                    {
                      Ptr<UdpClient> client = DynamicCast<UdpClient> (*it);
                      client->SetAttribute ("MaxPackets", UintegerValue (4294967295u));
                    }
                  for (auto it = downlinkClientApps.Begin (); it != downlinkClientApps.End (); ++it)
                    {
                      Ptr<UdpClient> client = DynamicCast<UdpClient> (*it);
                      client->SetAttribute ("MaxPackets", UintegerValue (4294967295u));
                    }
                }
            }
        }
      else if (((aggregateDownlinkMbps != 0) && isAp) || ((aggregateUplinkMbps != 0) && !isAp)) //UL or DL
        {
          nEstablishedAddaBa++;
          if (nEstablishedAddaBa == n * nBss)
            {
              allAddBaEstablished = true;
              timeAllAddBaEstablished = t;
              if (filterOutNonAddbaEstablished)
                {
                  Simulator::Stop (Seconds (duration));
                  for (auto it = uplinkClientApps.Begin (); it != uplinkClientApps.End (); ++it)
                    {
                      Ptr<UdpClient> client = DynamicCast<UdpClient> (*it);
                      client->SetAttribute ("MaxPackets", UintegerValue (4294967295u));
                    }
                  for (auto it = downlinkClientApps.Begin (); it != downlinkClientApps.End (); ++it)
                    {
                      Ptr<UdpClient> client = DynamicCast<UdpClient> (*it);
                      client->SetAttribute ("MaxPackets", UintegerValue (4294967295u));
                    }
                }
            }
        }
    }
  else if (state == OriginatorBlockAckAgreement::RESET)
    {
      //Make sure ADDBA establishment will be restarted
      Ptr<UdpClient> client;
      if (isAp && (aggregateDownlinkMbps != 0))
        {
          //TODO: only do this for the recipient
          for (uint32_t i = 0; i < n; i++)
            {
              client = DynamicCast<UdpClient> (allNodes.Get (ContextToNodeId (context))->GetApplication (i));
              NS_ASSERT (client != 0);
              client->SetAttribute ("MaxPackets", UintegerValue (1));
            }
        }
      if (!isAp && (aggregateUplinkMbps != 0))
        {
          client = DynamicCast<UdpClient> (allNodes.Get (ContextToNodeId (context))->GetApplication (0));
          NS_ASSERT (client != 0);
          client->SetAttribute ("MaxPackets", UintegerValue (1));
        }
    }
//std::cout<<"Addbacb"<<std::endl;
}

void
StaAssocCb (std::string context, Mac48Address bssid)
{
  uint32_t nodeId = ContextToNodeId (context);
  uint32_t appId = 0;
  uint32_t bss = 1;
  // Determine application ID from node ID
  for (; bss <= nBss; bss++)
    {
      if (nodeId <= (bss * n) + bss - 1)
        {
          appId = nodeId - bss;
          break;
        }
    }
  nAssociatedStasPerBss[bss - 1]++;
  if (nAssociatedStasPerBss[bss - 1] == n)
    {
      // All STAs of this BSS are associated, we can extend beacon interval if bianchi flag is enabled
      Ptr<WifiNetDevice> apDevice;
      if (bss == 1)
        {
          apDevice = apDeviceA.Get (0)->GetObject<WifiNetDevice> ();
        }
      else if (bss == 2)
        {
          apDevice = apDeviceB.Get (0)->GetObject<WifiNetDevice> ();
        }
      else if (bss == 3)
        {
          apDevice = apDeviceC.Get (0)->GetObject<WifiNetDevice> ();
        }
      else if (bss == 4)
        {
          apDevice = apDeviceD.Get (0)->GetObject<WifiNetDevice> ();
        }
      else if (bss == 5)
        {
          apDevice = apDeviceE.Get (0)->GetObject<WifiNetDevice> ();
        }
      else if (bss == 6)
        {
          apDevice = apDeviceF.Get (0)->GetObject<WifiNetDevice> ();
        }
      else //bss == 7
        {
          apDevice = apDeviceG.Get (0)->GetObject<WifiNetDevice> ();
        }
      Ptr<ApWifiMac> apWifiMac = apDevice->GetMac ()->GetObject<ApWifiMac> ();
      apWifiMac->SetAttribute ("BeaconInterval", TimeValue (MicroSeconds (beaconInterval)));
    }
  if (filterOutNonAddbaEstablished)
    {
      // Here, we make sure that there is at least one packet in the queue after association (paquets queued before are dropped)
      Ptr<UdpClient> client;
      if (aggregateUplinkMbps != 0)
        {
          client = DynamicCast<UdpClient> (uplinkClientApps.Get(appId));
          client->SetAttribute ("MaxPackets", UintegerValue (1));
        }
      if (aggregateDownlinkMbps != 0)
        {
          client = DynamicCast<UdpClient> (downlinkClientApps.Get(appId));
          client->SetAttribute ("MaxPackets", UintegerValue (1));
        }
    }
  nAssociatedStas++;
  if (nAssociatedStas == (n * nBss))
    {
      allStasAssociated = true;
      if (!filterOutNonAddbaEstablished)
        {
          Simulator::Stop (Seconds (duration));
        }
    }
}



void
SignalCb (std::string context, bool wifi, uint32_t senderNodeId, double rxPowerDbm, Time rxDuration)
{
  if (g_logArrivals)
    {
      SignalArrival arr;
      arr.m_time = Simulator::Now ();
      arr.m_duration = rxDuration;
      arr.m_nodeId = ContextToNodeId (context);
      arr.m_senderNodeId = senderNodeId;
      arr.m_wifi = wifi;
      arr.m_power = rxPowerDbm;
      g_arrivals.push_back (arr);
      g_arrivalsDurationCounter += rxDuration.GetSeconds ();
    }

  NS_LOG_DEBUG (context << " " << wifi << " " << senderNodeId << " " << rxPowerDbm << " " << rxDuration.GetSeconds () / 1000.0);
  uint32_t nodeId = ContextToNodeId (context);

  packetsReceivedPerNode[nodeId][senderNodeId] += 1;
  rssiPerNode[nodeId][senderNodeId] += rxPowerDbm;
//std::cout<<"Side "<<std::endl;
}

void
SaveSpectrumPhyStats (std::string filename, const std::vector<SignalArrival> &arrivals)
{
  // if we are not logging the stats, then return
  if (g_logArrivals == false)
    return;

  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ofstream::out | std::ofstream::trunc);
  outFile.setf (std::ios_base::fixed);
  outFile.flush ();

  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  outFile << "#time(s) nodeId type sender endTime(s) duration(ms)     powerDbm" << std::endl;
  for (std::vector<SignalArrival>::size_type i = 0; i != arrivals.size (); i++)
    {
      outFile << std::setprecision (9) << std::fixed << arrivals[i].m_time.GetSeconds () <<  " ";
      outFile << arrivals[i].m_nodeId << " ";
      outFile << ((arrivals[i].m_wifi == true) ? "wifi " : " lte ");
      outFile << arrivals[i].m_senderNodeId << " ";
      outFile <<  arrivals[i].m_time.GetSeconds () + arrivals[i].m_duration.GetSeconds () << " ";
      outFile << arrivals[i].m_duration.GetSeconds () * 1000.0 << " " << arrivals[i].m_power << std::endl;
    }
  outFile.close ();
}
// ---------------------------------------------------display CW and BO----------------------------------------------------------------------
void
CwTrace (std::string context, uint32_t oldVal, uint32_t newVal)
{
  //std::cout <<"CW "<< Simulator::Now ().GetMicroSeconds () << " " << ContextToNodeId (context) << " " << newVal << std::endl;
}
void
BackoffTrace (std::string context, uint32_t newVal)
{
  //std::cout  <<"BO " <<Simulator::Now ().GetMicroSeconds () << " " << ContextToNodeId (context) << " " << newVal << std::endl;
}
//--------------------------------------------------------------------------------------------------------------------------------------------
void
SchedulePhyLogConnect (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::SpectrumWifiPhy/SignalArrival", MakeCallback (&SignalCb));
}

void
SchedulePhyLogDisconnect (void)
{
  Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::SpectrumWifiPhy/SignalArrival", MakeCallback (&SignalCb));
}


void
ScheduleStateLogConnect (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback (&StateCb));
}

void
ScheduleAddbaStateLogConnect (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/BlockAckManager/AgreementState", MakeCallback (&AddbaStateCb));
}

void
ScheduleStaAssocLogConnect (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback (&StaAssocCb));
}

void
ScheduleStateLogDisconnect (void)
{
  Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback (&StateCb));
}

void
ScheduleAddbaStateLogDisconnect (void)
{
  Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/BlockAckManager/AgreementState", MakeCallback (&AddbaStateCb));
}

void
ScheduleStaAssocLogDisconnect (void)
{
  Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback (&StaAssocCb));
}

void AddClient (ApplicationContainer &clientApps, Ipv4Address address, Ptr<Node> node, uint16_t port, Time interval, uint32_t payloadSize, Ptr<UniformRandomVariable> urv, double txStartOffset)
{
  double next_rng = 0;
  if (txStartOffset > 0)
  {
    next_rng = urv->GetValue();
  }
  UdpClientHelper client (address, port);
  if (filterOutNonAddbaEstablished)
  {
    client.SetAttribute ("MaxPackets", UintegerValue (1));
  }
  else
  {
    client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  }
  client.SetAttribute ("Interval", TimeValue (interval + NanoSeconds (next_rng)));
  client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  clientApps.Add (client.Install (node));
}

void AddServer (ApplicationContainer &serverApps, UdpServerHelper &server, Ptr<Node> node)
{
  serverApps.Add (server.Install (node));
}

std::vector<uint32_t> signals (100);
std::vector<uint32_t> noises (100);

void
SaveSpatialReuseStats (const std::string filename, ////Multi-BSS scenario (BSS1,2)
                       const double d,
                       const double r1,
                       const double r2,
                       const double r3,
                       const double r4,
                       const double r5,
                       const double r6,
                       const int freqHz,
                       const double csr,
                       const std::string scenario,
                       double uplink,
                       double downlink)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ofstream::out | std::ofstream::trunc);
  outFile.setf (std::ios_base::fixed);
  outFile.flush ();

  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }

  uint32_t numNodes = packetsReceived.size ();
  uint32_t n = (numNodes / nBss) - 1;

  outFile << "Spatial Reuse Statistics" << std::endl;
  outFile << "Scenario: " << scenario << std::endl;
  outFile << "APs: " << nBss << std::endl;
  outFile << "Nodes per AP: " << n << std::endl;
  outFile << "Distance between APs [m]: " << d << std::endl;
  outFile << "BSS1_Radius [m]: " << r1 << std::endl; //Multi-BSS scenario (BSS1)
  outFile << "BSS2_Radius [m]: " << r2 << std::endl; //Multi-BSS scenario (BSS2)
  outFile << "BSS3_Radius [m]: " << r3 << std::endl; //Multi-BSS scenario (BSS3)
  outFile << "BSS4_Radius [m]: " << r4 << std::endl; //Multi-BSS scenario (BSS4)
  outFile << "BSS5_Radius [m]: " << r5 << std::endl; //Multi-BSS scenario (BSS5)
  outFile << "BSS6_Radius [m]: " << r6 << std::endl; //Multi-BSS scenario (BSS6)
  std::cout << "Spatial Reuse Statistics" << std::endl;
  std::cout << "Scenario: " << scenario << std::endl;
  std::cout << "APs: " << nBss << std::endl;
  std::cout << "Nodes per AP: " << n << std::endl;
  std::cout << "Distance between APs [m]: " << d << std::endl;

  std::cout << "BSS1_Radius [m]: " << r1 << std::endl; //Multi-BSS scenario (BSS1)
  std::cout << "BSS2_Radius [m]: " << r2 << std::endl; //Multi-BSS scenario (BSS2)
  std::cout << "BSS3_Radius [m]: " << r3 << std::endl; //Multi-BSS scenario (BSS3)
  std::cout << "BSS4_Radius [m]: " << r4 << std::endl; //Multi-BSS scenario (BSS4)
  std::cout << "BSS5_Radius [m]: " << r5 << std::endl; //Multi-BSS scenario (BSS5)
  std::cout << "BSS6_Radius [m]: " << r6 << std::endl; //Multi-BSS scenario (BSS6)
  std::cout << "Uplink [Mbps]: " << uplink << std::endl;
  std::cout << "Downlink [Mbps]: " << downlink << std::endl;

  double effectiveDuration;
  if (filterOutNonAddbaEstablished)
    {
      effectiveDuration = (timeLastPacketReceived - timeAllAddBaEstablished).GetNanoSeconds () / 1e9;
    }
  else
    {
      effectiveDuration = duration;
    }

  double tputApUplinkTotal = 0;
  double tputApDownlinkTotal = 0;
  for (uint32_t bss = 1; bss <= nBss; bss++)
    {
      uint64_t bytesReceivedApUplink = 0.0;
      uint64_t bytesReceivedApDownlink = 0.0;

      uint32_t bssFirstStaIdx = 0;
      bssFirstStaIdx = ((bss - 1) * n) + bss;
      uint32_t bssLastStaIdx = 0;
      bssLastStaIdx = bssFirstStaIdx + n;

      // uplink is the received bytes at the AP
      bytesReceivedApUplink += bytesReceived[bssFirstStaIdx - 1];

      // downlink is the sum of the received bytes for the STAs associated with the AP
      for (uint32_t k = bssFirstStaIdx; k < bssLastStaIdx; k++)
        {
          bytesReceivedApDownlink += bytesReceived[k];
        }

      double tputApUplink;
      double tputApDownlink;

      tputApUplink = static_cast<double>(bytesReceivedApUplink * 8) / 1e6 / effectiveDuration;
      tputApDownlink = static_cast<double>(bytesReceivedApDownlink * 8) / 1e6 / effectiveDuration;
      
      if (verifyResults && (bss == 1))
        {
          if ((tputApUplink < minExpectedThroughputBss1Up) || (tputApUplink > maxExpectedThroughputBss1Up))
            {
              NS_FATAL_ERROR ("Obtained upstream throughput for BSS1 " << tputApUplink << " is not expected!");
            }
          if ((tputApDownlink < minExpectedThroughputBss1Down) || (tputApDownlink > maxExpectedThroughputBss1Down))
            {
              NS_FATAL_ERROR ("Obtained downstream throughput for BSS1 " << tputApDownlink << " is not expected!");
            }
        }

      tputApUplinkTotal += tputApUplink;
      tputApDownlinkTotal += tputApDownlink;

      if (bss == 1)
        {
          // the efficiency, throughput / offeredLoad
          double uplinkEfficiency = 0.0;
          if (uplink > 0)
            {
              uplinkEfficiency = tputApUplink / uplink * 100.0;
            }
          if (uplinkEfficiency > 100.0)
            {
              uplinkEfficiency = 100.0;
            }
          double downlinkEfficiency = 0.0;
          if (downlink > 0)
            {
              downlinkEfficiency = tputApDownlink / downlink * 100.0;
            }
          if (downlinkEfficiency > 100.0)
            {
              downlinkEfficiency = 100.0;
            }
          std::cout << "Uplink Efficiency   " << uplinkEfficiency   << " [%]" << std::endl;
          std::cout << "Downlink Efficiency " << downlinkEfficiency << " [%]" << std::endl;
        }

      // TODO: debug to print out t-put, can remove
      std::cout << "Throughput,  AP" << bss << " Uplink   [Mbps] : " << tputApUplink << std::endl;
      std::cout << "Throughput,  AP" << bss << " Downlink [Mbps] : " << tputApDownlink << std::endl;

      outFile << "Throughput,  AP" << bss << " Uplink   [Mbps] : " << tputApUplink << std::endl;
      outFile << "Throughput,  AP" << bss << " Downlink [Mbps] : " << tputApDownlink << std::endl;

      double area1 = M_PI * r1 * r1; //Multi-BSS scenario (BSS1)
      double area2 = M_PI * r2 * r2; //Multi-BSS scenario (BSS2)
      double area3 = M_PI * r3 * r3; //Multi-BSS scenario (BSS3)
      double area4 = M_PI * r4 * r4; //Multi-BSS scenario (BSS4)
      double area5 = M_PI * r5 * r5; //Multi-BSS scenario (BSS5)
      double area6 = M_PI * r6 * r6; //Multi-BSS scenario (BSS6)
      outFile << "BSS1_Area Capacity, AP" << bss << " Uplink   [Mbps/m^2] : " << tputApUplink / area1 << std::endl; //Multi-BSS scenario (BSS1)
      outFile << "BSS1_Area Capacity, AP" << bss << " Downlink [Mbps/m^2] : " << tputApDownlink / area1 << std::endl;
      outFile << "BSS2_Area Capacity, AP" << bss << " Uplink   [Mbps/m^2] : " << tputApUplink / area2 << std::endl; //Multi-BSS scenario (BSS2)
      outFile << "BSS2_Area Capacity, AP" << bss << " Downlink [Mbps/m^2] : " << tputApDownlink / area2 << std::endl; 
      outFile << "BSS3_Area Capacity, AP" << bss << " Uplink   [Mbps/m^2] : " << tputApUplink / area3 << std::endl; //Multi-BSS scenario (BSS3)
      outFile << "BSS3_Area Capacity, AP" << bss << " Downlink [Mbps/m^2] : " << tputApDownlink / area3 << std::endl;
      outFile << "BSS4_Area Capacity, AP" << bss << " Uplink   [Mbps/m^2] : " << tputApUplink / area4 << std::endl; //Multi-BSS scenario (BSS4)
      outFile << "BSS4_Area Capacity, AP" << bss << " Downlink [Mbps/m^2] : " << tputApDownlink / area4 << std::endl; 
      outFile << "BSS5_Area Capacity, AP" << bss << " Uplink   [Mbps/m^2] : " << tputApUplink / area5 << std::endl; //Multi-BSS scenario (BSS5)
      outFile << "BSS5_Area Capacity, AP" << bss << " Downlink [Mbps/m^2] : " << tputApDownlink / area5 << std::endl;
      outFile << "BSS6_Area Capacity, AP" << bss << " Uplink   [Mbps/m^2] : " << tputApUplink / area6 << std::endl; //Multi-BSS scenario (BSS6)
      outFile << "BSS6_Area Capacity, AP" << bss << " Downlink [Mbps/m^2] : " << tputApDownlink / area6 << std::endl; 

      outFile << "Spectrum Efficiency, AP" << bss << " Uplink   [Mbps/Hz] : " << tputApUplink / freqHz << std::endl;
      outFile << "Spectrum Efficiency, AP" << bss << " Downlink [Mbps/Hz] : " << tputApDownlink / freqHz << std::endl;

      outFile << "Air-time utilization, AP" << bss << " [%] : " << (static_cast<double> (busyTime[bss - 1].GetNanoSeconds ()) / 1e9 / effectiveDuration) * 100 << std::endl;
    }

  std::cout << "Total Throughput Uplink   [Mbps] : " << tputApUplinkTotal << std::endl;
  std::cout << "Total Throughput Downlink [Mbps] : " << tputApDownlinkTotal << std::endl;

  outFile << "Total Throughput Uplink   [Mbps] : " << tputApUplinkTotal << std::endl;
  outFile << "Total Throughput Downlink [Mbps] : " << tputApDownlinkTotal << std::endl;
  
  double rxThroughputPerNode[numNodes];
  double txThroughputPerNode[numNodes];
  // output for all nodes
  for (uint32_t k = 0; k < numNodes; k++)
    {
      double bitsReceived = bytesReceived[k] * 8;
      double bitsTransmitted = bytesTransmitted[k] * 8;
      rxThroughputPerNode[k] = static_cast<double> (bitsReceived) / 1e6 / duration;
      txThroughputPerNode[k] = static_cast<double> (bitsTransmitted) / 1e6 / duration;
      outFile << "Node " << k << ", pkts " << packetsReceived[k] << ", bytes " << bytesReceived[k] << ", throughput [MMb/s] " << rxThroughputPerNode[k] << std::endl;
      outFile << "Node " << k << ", bytes " << bytesTransmitted[k] << ", throughput [MMb/s] " << txThroughputPerNode[k] << std::endl;
    }

  outFile << "Avg. RSSI:" << std::endl;
  for (uint32_t rxNodeId = 0; rxNodeId < numNodes; rxNodeId++)
    {
      for (uint32_t txNodeId = 0; txNodeId < numNodes; txNodeId++)
        {
          uint64_t pkts = packetsReceivedPerNode[rxNodeId][txNodeId];
          double rssi = rssiPerNode[rxNodeId][txNodeId];
          double avgRssi = 0.0;
          if (pkts > 0)
            {
              avgRssi = rssi / pkts;
            }
          outFile << avgRssi << "  ";
        }
      outFile << std::endl;
    }

  outFile << "CDF (dBm, signal, noise)" << std::endl;
  uint32_t signals_total = 0;
  uint32_t noises_total = 0;
  for (uint32_t idx = 0; idx < 100; idx++)
    {
      signals_total += signals[idx];
      noises_total += noises[idx];
    }

  uint32_t sum_signal = 0;
  uint32_t sum_noise = 0;
  // for dBm from -100 to -30
  for (uint32_t idx = 0; idx < 71; idx++)
    {
      sum_signal += signals[idx];
      sum_noise += noises[idx];
      outFile << ((double) idx - 100.0) << " " << (double) sum_signal / (double) signals_total << " " << (double) sum_noise / (double) noises_total << std::endl;
    }

  outFile.close ();

  std::cout << "Spatial Reuse Stats written to: " << filename << std::endl;

}

void ProcessPacket (std::string context,
                    Ptr<const Packet> packet,
                    uint16_t channelFreqMhz,
                    WifiTxVector txVector,
                    MpduInfo aMpdu,
                    SignalNoiseDbm signalNoise)
{
  uint32_t rxNodeId = ContextToNodeId (context);
  int dstNodeId = -1;
  int srcNodeId = -1;
  Mac48Address addr1;
  Mac48Address addr2;
  if (packet)
    {
      WifiMacHeader hdr;
      packet->PeekHeader (hdr);
      addr1 = hdr.GetAddr1 ();
      addr2 = hdr.GetAddr2 ();
      Mac48Address whatsThis = Mac48Address ("00:00:00:00:00:00");
      if (!addr1.IsBroadcast () && (addr2 != whatsThis))
        {
          dstNodeId = MacAddressToNodeId (addr1);
          srcNodeId = MacAddressToNodeId (addr2);
          g_samples++;
          g_signalDbmAvg += ((signalNoise.signal - g_signalDbmAvg) / g_samples);
          g_noiseDbmAvg += ((signalNoise.noise - g_noiseDbmAvg) / g_samples);
          Address rxNodeAddress = allNodes.Get (rxNodeId)->GetDevice (0)->GetAddress ();
          uint32_t pktSize = packet->GetSize ();
          g_rxSniffFile << rxNodeId << ", " << dstNodeId << ", " << srcNodeId << ", " << rxNodeAddress << ", " << addr1 << ", " << addr2 << ", " << signalNoise.noise << ", " << signalNoise.signal << ", " << pktSize << ", " << signalNoise.signal - signalNoise.noise << std::endl;
          if (signalNoise.signal < g_min_signal)
            {
              g_min_signal = signalNoise.signal;
            }
          if (signalNoise.signal > g_max_signal)
            {
              g_max_signal = signalNoise.signal;
            }
          if (signalNoise.noise < g_min_noise)
            {
              g_min_noise = signalNoise.noise;
            }
          if (signalNoise.noise > g_max_noise)
            {
              g_max_noise = signalNoise.noise;
            }
          uint32_t idx = floor (signalNoise.signal) + 100;
          signals[idx]++;
          idx = floor (signalNoise.noise) + 100;
          noises[idx]++;
        }
    }
}

// void MonitorSniffRx (std::string context,
//                      Ptr<const Packet> packet,
//                      uint16_t channelFreqMhz,
//                      WifiTxVector txVector,
//                      MpduInfo aMpdu,
//                      SignalNoiseDbm signalNoise)
// {
//   if (packet)
//     {
//       Ptr <Packet> packetCopy = packet->Copy();
//       AmpduTag ampdu;
//       if (packetCopy->RemovePacketTag (ampdu))
//         {
//           // A-MPDU frame
//           MpduAggregator::DeaggregatedMpdus packets = MpduAggregator::Deaggregate (packetCopy);
//           for (MpduAggregator::DeaggregatedMpdusCI n = packets.begin (); n != packets.end (); ++n)
//             {
//               std::pair<Ptr<Packet>, AmpduSubframeHeader> deAggPair = (std::pair<Ptr<Packet>, AmpduSubframeHeader>) * n;
//               Ptr<Packet> aggregatedPacket = deAggPair.first;
//               ProcessPacket (context,
//                              aggregatedPacket,
//                              channelFreqMhz,
//                              txVector,
//                              aMpdu,
//                              signalNoise);
//             }
//         }
//       else
//         {
//           ProcessPacket (context,
//                          packet,
//                          channelFreqMhz,
//                          txVector,
//                          aMpdu,
//                          signalNoise);
//         }
//     }
// }

void
SaveUdpFlowMonitorStats (std::string filename, std::string simulationParams, Ptr<FlowMonitor> monitor, FlowMonitorHelper& flowmonHelper, double duration)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ofstream::out | std::ofstream::app);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  outFile.setf (std::ios_base::fixed);

  // Print per-flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

      outFile << i->first
              << " " << t.sourceAddress << ":" << t.sourcePort
              << " " << t.destinationAddress << ":" << t.destinationPort
              << " " << i->second.txPackets
              << " " << i->second.txBytes
              // Mb/s
              << " " << (i->second.txBytes * 8.0 / duration) / 1e6;
      if (i->second.rxPackets > 0)
        {
          // Measure the duration of the flow from receiver's perspective
          double rxDuration = i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ();
          // Mb/s
          outFile << " " << i->second.rxBytes
                  // Mb/s
                  << " " << (i->second.rxBytes * 8.0 / rxDuration) / 1e6
                  // milliseconds
                  << " " << 1000 * i->second.delaySum.GetSeconds () / i->second.rxPackets
                  << " " << 1000 * i->second.jitterSum.GetSeconds () / i->second.rxPackets;
        }
      else
        {
          outFile << "  0" // rxBytes
                  << "  0" // throughput
                  << "  0" // delaySum
                  << "  0"; // jitterSum
        }
      outFile << " " << i->second.rxPackets << std::endl;
    }
  outFile.close ();
}

void PopulateArpCache ()
{
  // Creates ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  
  // Set ARP Timeout
  arp->SetAliveTimeout (Seconds(3600 * 24 * 365)); // 1-year
  
  // Populates ARP Cache with information from all nodes
  for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
  {
    // Get an interactor to Ipv4L3Protocol instance
    Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
    //NS_ASSERT(ip !=0);
    if (ip == 0) continue;
    // Get interfaces list from Ipv4L3Protocol iteractor
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);
    
    // For each interface
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
    {
      // Get an interactor to Ipv4L3Protocol instance
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
      NS_ASSERT(ipIface != 0);
      
      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);
      
      // Get MacAddress assigned to this device
      Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress ());
      
      // For each Ipv4Address in the list of Ipv4Addresses assign to this interface...
      for(uint32_t k = 0; k < ipIface->GetNAddresses (); k++)
      {
        // Get Ipv4Address
        Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();
        
        // If Loopback address, go to the next
        if(ipAddr == Ipv4Address::GetLoopback())
          continue;
        
        // Creates an ARP entry for this Ipv4Address and adds it to the ARP Cache
        ArpCache::Entry * entry = arp->Add(ipAddr);
        Ptr<Packet> pkt = Create<Packet> ();
        Ipv4Header hdr;
        entry->MarkWaitReply(ArpCache::Ipv4PayloadHeaderPair (pkt, hdr));
        entry->MarkAlive(addr);
      }
    }
  }
  
  // Assign ARP Cache to each interface of each node
  for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
  {
    Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
    //NS_ASSERT(ip !=0);
    if (ip == 0) continue;
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
      ipIface->SetAttribute("ArpCache", PointerValue(arp));
    }
  }
}

// main script
int
main (int argc, char *argv[])
{
  // command line configurable parameters
  int maxSlrc = 7;
  bool enablePcap = false;
  bool enableAscii = false;
  double powSta = 10.0; // dBm
  double powAp = 21.0; // dBm
  double ccaTrSta = -62; // dBm(default)
  double ccaTrAp = -62; // dBm
  double ccaTrSta1 = 0; // Multi-BSS scenario (BSS1)：スクリプト内で定義するためとりあえず0に設定
  double ccaTrAp1 = 0; 
  double ccaTrSta2 = 0; // Multi-BSS scenario (BSS2)
  double ccaTrAp2 = 0; 
  double ccaTrSta3 = 0; // Multi-BSS scenario (BSS3)：スクリプト内で定義するためとりあえず0に設定
  double ccaTrAp3 = 0; 
  double ccaTrSta4 = 0; // Multi-BSS scenario (BSS4)
  double ccaTrAp4 = 0;
  double ccaTrSta5 = 0; // Multi-BSS scenario (BSS5)：スクリプト内で定義するためとりあえず0に設定
  double ccaTrAp5 = 0; 
  double ccaTrSta6 = 0; // Multi-BSS scenario (BSS6)
  double ccaTrAp6 = 0;
  double d = 100.0; // distance between AP1 and AP2, m
  double r = 50.0; // radius of circle around each AP in which to scatter the STAs(default)
  double r1 = 50.0; // Multi-BSS scenario (BSS1)
  double r2 = 50.0; // Multi-BSS scenario (BSS2)
  double r3 = 50.0; // Multi-BSS scenario (BSS3)
  double r4 = 50.0; // Multi-BSS scenario (BSS4)
  double r5 = 50.0; // Multi-BSS scenario (BSS5)
  double r6 = 50.0; // Multi-BSS scenario (BSS6)
  bool enableRts = 0;
  double txRange = 54.0; // [m]
  int bw = 20;
  std::string standard ("11ax_5GHZ");
  double csr = 1000.0; // carrier sense range
  double txStartOffset = 5.0; // [ns]
  double obssPdThresholdBss1 = -99.0;
  double obssPdThresholdBss2 = -99.0;
  double obssPdThresholdBss3 = -99.0;
  double obssPdThresholdBss4 = -99.0;
  double obssPdThresholdBss5 = -99.0;
  double obssPdThresholdBss6 = -99.0;
  double obssPdThresholdBss7 = -99.0;
  double obssPdThresholdMinBss1 = -82.0;
  double obssPdThresholdMinBss2 = -82.0;
  double obssPdThresholdMinBss3 = -82.0;
  double obssPdThresholdMinBss4 = -82.0;
  double obssPdThresholdMinBss5 = -82.0;
  double obssPdThresholdMinBss6 = -82.0;
  double obssPdThresholdMinBss7 = -82.0;
  double obssPdThresholdMaxBss1 = -62.0;
  double obssPdThresholdMaxBss2 = -62.0;
  double obssPdThresholdMaxBss3 = -62.0;
  double obssPdThresholdMaxBss4 = -62.0;
  double obssPdThresholdMaxBss5 = -62.0;
  double obssPdThresholdMaxBss6 = -62.0;
  double obssPdThresholdMaxBss7 = -62.0;
  double txGain = 0.0; // dBi
  double rxGain = 0.0; // dBi
  uint16_t antennas = 1;
  uint16_t maxSupportedTxSpatialStreams = 1;
  uint16_t maxSupportedRxSpatialStreams = 1;
  uint32_t performTgaxTimingChecks = 0;
  // the scenario - should be one of: residential, enterprise, indoor, or outdoor
  std::string scenario ("residential");
  std::string obssPdAlgorithm ("DynamicObssPdAlgorithm");
  std::string testname ("test");

  // local variables
  std::string outputFilePrefix = "spatial-reuse";
  uint32_t payloadSizeUplink = 1500; // bytes
  uint32_t payloadSizeDownlink = 300; // bytes
  uint16_t mcs = 0; // MCS value (if constant rate)
  uint16_t mcs1 = 8; // Multi-BSS scenario (BSS1)
  uint16_t mcs2 = 8; // Multi-BSS scenario (BSS2)
  uint16_t mcs3 = 8; // Multi-BSS scenario (BSS3)
  uint16_t mcs4 = 8; // Multi-BSS scenario (BSS4)
  uint16_t mcs5 = 8; // Multi-BSS scenario (BSS5)
  uint16_t mcs6 = 8; // Multi-BSS scenario (BSS6)
  Time interval = MicroSeconds (1000);
  bool enableObssPd = false;
  uint32_t maxAmpduSizeBss1 = 65535;
  uint32_t maxAmpduSizeBss2 = 65535;
  uint32_t maxAmpduSizeBss3 = 65535;
  uint32_t maxAmpduSizeBss4 = 65535;
  uint32_t maxAmpduSizeBss5 = 65535;
  uint32_t maxAmpduSizeBss6 = 65535;
  uint32_t maxAmpduSizeBss7 = 65535;
  std::string nodePositionsFile ("");
  double applicationTxStart = 1.0; // brief delay (s) for the network to settle into a steady state before applications start sending packets
  bool useIdealWifiManager = false;
  bool bianchi = false;
  double sigma = 5.0;
  double rxSensitivity = -91.0;
  uint32_t maxMissedBeacons = 10;
  bool useExplicitBarAfterMissedBlockAck = true;
  uint64_t maxQueueDelay = 500; // milliSeconds
  bool enableFrameCapture = false;
  bool enableThresholdPreambleDetection = true;
  bool disableArp = true;
  uint16_t colorBss1 = 1;
  uint16_t colorBss2 = 2;
  uint16_t colorBss3 = 3;
  uint16_t colorBss4 = 4;
  uint16_t colorBss5 = 5;
  uint16_t colorBss6 = 6;
  uint16_t colorBss7 = 7;
  bool powerBackoff = true;

  CommandLine cmd;
  cmd.AddValue ("duration", "Duration of simulation (s)", duration);
  cmd.AddValue ("applicationTxStart", "Time (s) to allow network to reach steady-state before applications start sending packets.", applicationTxStart);
  cmd.AddValue ("powSta", "Power of STA (dBm)", powSta);
  cmd.AddValue ("powAp", "Power of AP (dBm)", powAp);
  cmd.AddValue ("txGain", "Transmission gain (dB)", txGain);
  cmd.AddValue ("rxGain", "Reception gain (dB)", rxGain);
  cmd.AddValue ("antennas", "The number of antennas on each device.", antennas);
  cmd.AddValue ("maxSupportedTxSpatialStreams", "The maximum number of supported Tx spatial streams.", maxSupportedTxSpatialStreams);
  cmd.AddValue ("maxSupportedRxSpatialStreams", "The maximum number of supported Rx spatial streams.", maxSupportedRxSpatialStreams);
  cmd.AddValue ("ccaTrSta", "CCA-ED Threshold of STA (dBm)", ccaTrSta); //default
  cmd.AddValue ("ccaTrAp", "CCA-ED Threshold of AP (dBm)", ccaTrAp);
  cmd.AddValue ("ccaTrSta1", "BSS1_CCA-ED Threshold of STA (dBm)", ccaTrSta1); //Multi-BSS scenario (BSS1)
  cmd.AddValue ("ccaTrAp1", "BSS1_CCA-ED Threshold of AP (dBm)", ccaTrAp1);
  cmd.AddValue ("ccaTrSta2", "BSS2_CCA-ED Threshold of STA (dBm)", ccaTrSta2); //Multi-BSS scenario (BSS2)
  cmd.AddValue ("ccaTrAp2", "BSS2_CCA-ED Threshold of AP (dBm)", ccaTrAp2);
  cmd.AddValue ("ccaTrSta3", "BSS1_CCA-ED Threshold of STA (dBm)", ccaTrSta3); //Multi-BSS scenario (BSS3)
  cmd.AddValue ("ccaTrAp3", "BSS1_CCA-ED Threshold of AP (dBm)", ccaTrAp3);
  cmd.AddValue ("ccaTrSta4", "BSS2_CCA-ED Threshold of STA (dBm)", ccaTrSta4); //Multi-BSS scenario (BSS4)
  cmd.AddValue ("ccaTrAp4", "BSS2_CCA-ED Threshold of AP (dBm)", ccaTrAp4);
  cmd.AddValue ("ccaTrSta5", "BSS1_CCA-ED Threshold of STA (dBm)", ccaTrSta5); //Multi-BSS scenario (BSS5)
  cmd.AddValue ("ccaTrAp5", "BSS1_CCA-ED Threshold of AP (dBm)", ccaTrAp5);
  cmd.AddValue ("ccaTrSta6", "BSS2_CCA-ED Threshold of STA (dBm)", ccaTrSta6); //Multi-BSS scenario (BSS6)
  cmd.AddValue ("ccaTrAp6", "BSS2_CCA-ED Threshold of AP (dBm)", ccaTrAp6);
  cmd.AddValue ("rxSensitivity", "Receiver Sensitivity (dBm)", rxSensitivity);
  cmd.AddValue ("d", "Distance between AP1 and AP2 (m)", d);
  cmd.AddValue ("n", "Number of STAs to scatter around each AP", n);
  cmd.AddValue ("r", "Radius of circle around each AP in which to scatter STAs (m)", r); //default
  cmd.AddValue ("r1", "BSS_1Radius of circle around each AP in which to scatter STAs (m)", r1); //Multi-BSS scenario (BSS1)
  cmd.AddValue ("r2", "BSS_2Radius of circle around each AP in which to scatter STAs (m)", r2); //Multi-BSS scenario (BSS2)
  cmd.AddValue ("r3", "BSS_1Radius of circle around each AP in which to scatter STAs (m)", r3); //Multi-BSS scenario (BSS3)
  cmd.AddValue ("r4", "BSS_2Radius of circle around each AP in which to scatter STAs (m)", r4); //Multi-BSS scenario (BSS4)
  cmd.AddValue ("r5", "BSS_1Radius of circle around each AP in which to scatter STAs (m)", r5); //Multi-BSS scenario (BSS5)
  cmd.AddValue ("r6", "BSS_2Radius of circle around each AP in which to scatter STAs (m)", r6); //Multi-BSS scenario (BSS6)
  cmd.AddValue ("uplink", "Aggregate uplink load, STAs-AP (Mbps)", aggregateUplinkMbps);
  cmd.AddValue ("downlink", "Aggregate downlink load, AP-STAs (Mbps)", aggregateDownlinkMbps);
  cmd.AddValue ("standard", "Set standard (802.11a, 802.11b, 802.11g, 802.11n-5GHz, 802.11n-2.4GHz, 802.11ac, 802.11-holland, 802.11-10MHz, 802.11-5MHz, 802.11ax-5GHz, 802.11ax-2.4GHz)", standard);
  cmd.AddValue ("bw", "Bandwidth (consistent with standard, in MHz)", bw);
  cmd.AddValue ("enableObssPd", "Enable OBSS_PD", enableObssPd);
  cmd.AddValue ("csr", "Carrier Sense Range (CSR) [m]", csr);
  cmd.AddValue ("enableRts", "Enable or disable RTS/CTS", enableRts);
  cmd.AddValue ("maxSlrc", "MaxSlrc", maxSlrc);
  cmd.AddValue ("txRange", "Max TX range [m]", txRange);
  cmd.AddValue ("payloadSizeUplink", "Payload size of 1 uplink packet [bytes]", payloadSizeUplink);
  cmd.AddValue ("payloadSizeDownlink", "Payload size of 1 downlink packet [bytes]", payloadSizeDownlink);
  cmd.AddValue ("MCS", "Modulation and Coding Scheme (MCS) index (default=0)", mcs); //default
  cmd.AddValue ("MCS1", "BSS1_Modulation and Coding Scheme (MCS) index (default=0)", mcs1); //Multi-BSS scenario (BSS1)
  cmd.AddValue ("MCS2", "BSS2_Modulation and Coding Scheme (MCS) index (default=0)", mcs2); //Multi-BSS scenario (BSS2)
  cmd.AddValue ("MCS3", "BSS1_Modulation and Coding Scheme (MCS) index (default=0)", mcs3); //Multi-BSS scenario (BSS3)
  cmd.AddValue ("MCS4", "BSS2_Modulation and Coding Scheme (MCS) index (default=0)", mcs4); //Multi-BSS scenario (BSS4)
  cmd.AddValue ("MCS5", "BSS1_Modulation and Coding Scheme (MCS) index (default=0)", mcs5); //Multi-BSS scenario (BSS5)
  cmd.AddValue ("MCS6", "BSS2_Modulation and Coding Scheme (MCS) index (default=0)", mcs6); //Multi-BSS scenario (BSS6)
  cmd.AddValue ("txStartOffset", "N(0, mu) offset for each node's start of packet transmission.  Default mu=5 [ns]", txStartOffset);
  cmd.AddValue ("obssPdThresholdBss1", "Energy threshold (dBm) for BSS 1 of received signal below which the PHY layer can avoid declaring CCA BUSY for inter-BSS frames.", obssPdThresholdBss1);
  cmd.AddValue ("obssPdThresholdBss2", "Energy threshold (dBm) for BSS 2 of received signal below which the PHY layer can avoid declaring CCA BUSY for inter-BSS frames.", obssPdThresholdBss2);
  cmd.AddValue ("obssPdThresholdBss3", "Energy threshold (dBm) for BSS 3 of received signal below which the PHY layer can avoid declaring CCA BUSY for inter-BSS frames.", obssPdThresholdBss3);
  cmd.AddValue ("obssPdThresholdBss4", "Energy threshold (dBm) for BSS 4 of received signal below which the PHY layer can avoid declaring CCA BUSY for inter-BSS frames.", obssPdThresholdBss4);
  cmd.AddValue ("obssPdThresholdBss5", "Energy threshold (dBm) for BSS 5 of received signal below which the PHY layer can avoid declaring CCA BUSY for inter-BSS frames.", obssPdThresholdBss5);
  cmd.AddValue ("obssPdThresholdBss6", "Energy threshold (dBm) for BSS 6 of received signal below which the PHY layer can avoid declaring CCA BUSY for inter-BSS frames.", obssPdThresholdBss6);
  cmd.AddValue ("obssPdThresholdBss7", "Energy threshold (dBm) for BSS 7 of received signal below which the PHY layer can avoid declaring CCA BUSY for inter-BSS frames.", obssPdThresholdBss7);
  cmd.AddValue ("checkTimings", "Perform TGax timings checks (for MAC simulation calibrations).", performTgaxTimingChecks);
  cmd.AddValue ("scenario", "The spatial-reuse scenario (residential, enterprise, indoor or outdoor).", scenario);
  cmd.AddValue ("obssPdAlgorithm", "OBSS PD Algorithm (ConstantObssPdAlgorithm, DynamicObssPdAlgorithm).", obssPdAlgorithm);
  cmd.AddValue ("nBss", "The number of BSSs.", nBss);
  cmd.AddValue ("maxAmpduSizeBss1", "The maximum A-MPDU size for BSS 1 (bytes).", maxAmpduSizeBss1);
  cmd.AddValue ("maxAmpduSizeBss2", "The maximum A-MPDU size for BSS 2 (bytes).", maxAmpduSizeBss2);
  cmd.AddValue ("maxAmpduSizeBss3", "The maximum A-MPDU size for BSS 3 (bytes).", maxAmpduSizeBss3);
  cmd.AddValue ("maxAmpduSizeBss4", "The maximum A-MPDU size for BSS 4 (bytes).", maxAmpduSizeBss4);
  cmd.AddValue ("maxAmpduSizeBss5", "The maximum A-MPDU size for BSS 5 (bytes).", maxAmpduSizeBss5);
  cmd.AddValue ("maxAmpduSizeBss6", "The maximum A-MPDU size for BSS 6 (bytes).", maxAmpduSizeBss6);
  cmd.AddValue ("maxAmpduSizeBss7", "The maximum A-MPDU size for BSS 7 (bytes).", maxAmpduSizeBss7);
  cmd.AddValue ("nodePositionsFile", "Node positions file, ns-2 format for Ns2MobilityHelper.", nodePositionsFile);
  cmd.AddValue ("enablePcap", "Enable PCAP trace file generation.", enablePcap);
  cmd.AddValue ("enableAscii", "Enable ASCII trace file generation.", enableAscii);
  cmd.AddValue ("useIdealWifiManager", "Use IdealWifiManager instead of ConstantRateWifiManager", useIdealWifiManager);
  cmd.AddValue ("bianchi", "Set parameters for Biachi validation", bianchi);
  cmd.AddValue ("test", "The testname.", testname);
  cmd.AddValue ("sigma", "Log-normal shadowing loss parameter.", sigma);
  cmd.AddValue ("beaconInterval", "Beacon interval in microseconds.", beaconInterval);
  cmd.AddValue ("maxMissedBeacons", "Number of beacons which much be consecutively missed before association is restarted", maxMissedBeacons);
  cmd.AddValue ("filterOutNonAddbaEstablished", "Flag whether statistics obtained before all ADDBA hanshakes have been established are filtered out.", filterOutNonAddbaEstablished);
  cmd.AddValue ("useExplicitBarAfterMissedBlockAck", "Flag whether explicit Block Ack Request should be sent upon missed Block Ack Response.", useExplicitBarAfterMissedBlockAck);
  cmd.AddValue ("maxQueueDelay", "If a packet stays longer than this delay in the queue, it is dropped.", maxQueueDelay);
  cmd.AddValue ("enableFrameCapture", "Enable or disable frame capture", enableFrameCapture);
  cmd.AddValue ("enableThresholdPreambleDetection", "Enable or disable threshold-based preamble detection (if not set, preamble detection is always successful)", enableThresholdPreambleDetection);
  cmd.AddValue ("disableArp", "Flag whether we disable ARP mechanism (populate cache before simulation starts and set a very high timeout).", disableArp);
  cmd.AddValue ("colorBss1", "The color for BSS 1.", colorBss1);
  cmd.AddValue ("colorBss2", "The color for BSS 2.", colorBss2);
  cmd.AddValue ("colorBss3", "The color for BSS 3.", colorBss3);
  cmd.AddValue ("colorBss4", "The color for BSS 4.", colorBss4);
  cmd.AddValue ("colorBss5", "The color for BSS 5.", colorBss5);
  cmd.AddValue ("colorBss6", "The color for BSS 6.", colorBss6);
  cmd.AddValue ("colorBss7", "The color for BSS 7.", colorBss7);
  cmd.AddValue ("powerBackoff", "Enable/disable OBSS_PD SR power backoff.", powerBackoff);
  cmd.AddValue ("verifyResults", "Flag used by regression to indicate resuts should be verified at the end of the simulation run.", verifyResults);
  cmd.AddValue ("minExpectedThroughputBss1Up", "The minimum expected upstream throughput in Mbps for BSS 1 (used by regression).", minExpectedThroughputBss1Up);
  cmd.AddValue ("maxExpectedThroughputBss1Up", "The maximum expected upstream throughput in Mbps for BSS 1 (used by regression).", maxExpectedThroughputBss1Up);
  cmd.AddValue ("minExpectedThroughputBss1Down", "The minimum expected downstream throughput in Mbps for BSS 1 (used by regression).", minExpectedThroughputBss1Down);
  cmd.AddValue ("maxExpectedThroughputBss1Down", "The maximum expected downstream throughput in Mbps for BSS 1 (used by regression).", maxExpectedThroughputBss1Down);
  cmd.Parse (argc, argv);

  if (!powerBackoff)
    {
      obssPdThresholdMinBss1 = obssPdThresholdBss1;
      obssPdThresholdMinBss2 = obssPdThresholdBss2;
      obssPdThresholdMinBss3 = obssPdThresholdBss3;
      obssPdThresholdMinBss4 = obssPdThresholdBss4;
      obssPdThresholdMinBss5 = obssPdThresholdBss5;
      obssPdThresholdMinBss6 = obssPdThresholdBss6;
      obssPdThresholdMinBss7 = obssPdThresholdBss7;
    }

  if (bianchi)
    {
      filterOutNonAddbaEstablished = false;
      maxQueueDelay = duration * 1000;
      disableArp = true;
      useExplicitBarAfterMissedBlockAck = false;
      beaconInterval = std::min<uint64_t> ((ceil ((duration * 1000000) / 1024) * 1024), (65535 * 1024)); //beacon interval needs to be a multiple of time units (1024 us)
      maxMissedBeacons = 4294967295;
    }
  
  if (filterOutNonAddbaEstablished)
    {
      maxQueueDelay = duration * 1000; //make sure there is no MSDU lifetime expired
      disableArp = true;
    }

  if ((nBss < 1) || (nBss > 7))
    {
      std::cout << "Invalid nBss parameter: " << nBss << ".  Can only be 1, 2, 3 or 4." << std::endl;
    }

  if (enableRts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
    }

  uint32_t uMaxSlrc = 7;
  if (maxSlrc < 0)
    {
      uMaxSlrc = std::numeric_limits<uint32_t>::max ();
    }
  else
    {
      uMaxSlrc = maxSlrc;
    }
  Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSlrc", UintegerValue (uMaxSlrc));

  Config::SetDefault ("ns3::QosTxop::UseExplicitBarAfterMissedBlockAck", BooleanValue (useExplicitBarAfterMissedBlockAck));

  // debugging - may need to set additional params specifically for Bianchi validation
  if (bianchi)
  {
    uMaxSlrc = std::numeric_limits<uint32_t>::max ();
    Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSlrc", UintegerValue (uMaxSlrc));
    Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSsrc", UintegerValue (uMaxSlrc));
  }

  Config::SetDefault ("ns3::WifiMacQueue::MaxDelay", TimeValue (MilliSeconds (maxQueueDelay)));
///////////////////////////////////////////////CS/CCAの計算、周波数、帯域幅の設定（ここから）//////////////////////////////////////////////////////////////////////////////
  std::ostringstream ossMcs; //default
  ossMcs << mcs;

  std::ostringstream ossMcs1; //Multi-BSS scenario (BSS1)
  ossMcs1 << mcs1;
  std::ostringstream ossMcs2; //Multi-BSS scenario (BSS2)
  ossMcs2 << mcs2; 
  std::ostringstream ossMcs3; //Multi-BSS scenario (BSS3)
  ossMcs3 << mcs3;
  std::ostringstream ossMcs4; //Multi-BSS scenario (BSS4)
  ossMcs4 << mcs4; 
  std::ostringstream ossMcs5; //Multi-BSS scenario (BSS5)
  ossMcs5 << mcs5;
  std::ostringstream ossMcs6; //Multi-BSS scenario (BSS6)
  ossMcs6 << mcs6; 
  // carrier sense range (csr) is a calculated value that is used for displaying the
  // estimated range in which an AP can successfully receive from STAs.
  // first, let us calculate the max distance, txRange, that a transmitting STA's signal
  // can be received above the CcaEdThreshold.
  // for simple calculation, we assume Friis propagation loss, CcaEdThreshold = -102dBm,
  // freq = 5.9GHz, no antenna gains, txPower = 10 dBm. the resulting value is:
  // calculation of Carrier Sense Range (CSR).
  // see: https://onlinelibrary.wiley.com/doi/pdf/10.1002/wcm.264
  // In (8), the optimum CSR = D x S0 ^ (1 / gamma)
  // S0 (the  min. SNR value for decoding a particular rate MCS) is calculated for several MCS values.
  // For reference, please see the  802.11ax Evaluation Methodology document (Appendix 3).
  // https://mentor.ieee.org/802.11/dcn/14/11-14-0571-12-00ax-evaluation-methodology.do
  //For the following conditions:
  // AWGN Channel
  // BCC with 1482 bytes Packet Length
  // PER=0.1
  // the minimum SINR values (S0) are
  //
  // [MCS S0 ]
  // [0 0.7dB]
  // [1 3.7dB]
  // [2  6.2dB]
  // [3 9.3dB]
  // [4 12.6dB]
  // [5 16.8dB]
  // [6 18.2dB]
  // [7 19.4dB]
  // [8 23.5dB]
  // [9 25.1dB]
  // Calculating CSR for MCS0, assuming gamma = 3, we get
  double s0 = 0.7;
  double gamma = 3.0;
  csr = txRange * pow (s0, 1.0 / gamma); //CS/CCAの計算
  //std::cout << "CS/CCA[m] = " << csr << std::endl;

  WifiHelper wifi;
  std::string dataRate; //default
  std::string dataRate1; //Multi-BSS scenario (BSS1)
  std::string dataRate2; //Multi-BSS scenario (BSS2)
  std::string dataRate3; //Multi-BSS scenario (BSS3)
  std::string dataRate4; //Multi-BSS scenario (BSS4)
  std::string dataRate5; //Multi-BSS scenario (BSS5)
  std::string dataRate6; //Multi-BSS scenario (BSS6)
  int freq;
  if (standard == "11a")
    {
      wifi.SetStandard (WIFI_STANDARD_80211a);
      dataRate = "OfdmRate6Mbps";
      freq = 5180;
      if (bw != 20)
        {
          std::cout << "Bandwidth is not compatible with standard" << std::endl;
          return 1;
        }
    }
  // else if (standard == "11_10MHZ")
  //   {
  //     wifi.SetStandard (WIFI_PHY_STANDARD_80211_10MHZ);
  //     dataRate = "OfdmRate3MbpsBW10MHz";
  //     freq = 5860;
  //     if (bw != 10)
  //       {
  //         std::cout << "Bandwidth is not compatible with standard" << std::endl;
  //         return 1;
  //       }
  //   }
  // else if (standard == "11_5MHZ")
  //   {
  //     wifi.SetStandard (WIFI_PHY_STANDARD_80211_5MHZ);
  //     dataRate = "OfdmRate1_5MbpsBW5MHz";
  //     freq = 5860;
  //     if (bw != 5)
  //       {
  //         std::cout << "Bandwidth is not compatible with standard" << std::endl;
  //         return 1;
  //       }
  //   }
  // else if (standard == "11n_2_4GHZ")
  //   {
  //     wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
  //     dataRate = "HtMcs" + ossMcs.str ();
  //     freq = 2402 + (bw / 2); //so as to have 2412/2422 for 20/40
  //     if (bw != 20 && bw != 40)
  //       {
  //         std::cout << "Bandwidth is not compatible with standard" << std::endl;
  //         return 1;
  //       }
  //   }
  // else if (standard == "11n_5GHZ")
  //   {
  //     wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  //     dataRate = "HtMcs" + ossMcs.str ();
  //     freq = 5170 + (bw / 2); //so as to have 5180/5190 for 20/40
  //     if (bw != 20 && bw != 40)
  //       {
  //         std::cout << "Bandwidth is not compatible with standard" << std::endl;
  //         return 1;
  //       }
  //   }
  else if (standard == "11ac") //spatial-reuseではここが実行
    {
      // wifi.SetStandard (WIFI_PHY_STANDARD_80211ac);
      wifi.SetStandard (WIFI_STANDARD_80211ac);
      dataRate1 = "VhtMcs" + ossMcs1.str ();  //Multi-BSS scenario (BSS1)
      dataRate2 = "VhtMcs" + ossMcs2.str ();  //Multi-BSS scenario (BSS2)
      dataRate3 = "VhtMcs" + ossMcs3.str ();  //Multi-BSS scenario (BSS3)
      dataRate4 = "VhtMcs" + ossMcs4.str ();  //Multi-BSS scenario (BSS4)
      dataRate5 = "VhtMcs" + ossMcs5.str ();  //Multi-BSS scenario (BSS5)
      dataRate6 = "VhtMcs" + ossMcs6.str ();  //Multi-BSS scenario (BSS6)
      freq = 5170 + (bw / 2); //so as to have 5180/5190/5210/5250 for 20/40/80/160
      if (bw != 20 && bw != 40 && bw != 80 && bw != 160)
        {
          std::cout << "Bandwidth is not compatible with standard" << std::endl;
          return 1;
        }
    }
  // else if (standard == "11ax_2_4GHZ")
  //   {
  //     wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_2_4GHZ);
  //     dataRate = "HeMcs" + ossMcs.str ();
  //     freq = 2402 + (bw / 2); //so as to have 2412/2422/2442 for 20/40/80
  //     if (bw != 20 && bw != 40 && bw != 80)
  //       {
  //         std::cout << "Bandwidth is not compatible with standard" << std::endl;
  //         return 1;
  //       }
  //   }
  // else if (standard == "11ax_5GHZ")
  //   {
  //     wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_5GHZ);
  //     dataRate = "HeMcs" + ossMcs.str ();
  //     freq = 5170 + (bw / 2); //so as to have 5180/5190/5210/5250 for 20/40/80/160
  //     if (bw != 20 && bw != 40 && bw != 80 && bw != 160)
  //       {
  //         std::cout << "Bandwidth is not compatible with standard" << std::endl;
  //         return 1;
  //       }
  //   }
  // else
  //   {
  //     std::cout << "Unknown OFDM standard (please refer to the listed possible values)" << std::endl;
  //     return 1;
  //   }

  // // disable ObssPd if not 11ax
  // if ((standard != "11ax_2_4GHZ") && (standard != "11ax_5GHZ"))
  //   {
  //     enableObssPd = false;
  //   }

  // total expected nodes.  n STAs for each AP
/////////////////////////////////////////////////////ノードの生成（ここから）//////////////////////////////////////////////////////////////////
  uint32_t numNodes = nBss * (n + 1);
  packetsReceived = std::vector<uint64_t> (numNodes);
  bytesReceived = std::vector<uint64_t> (numNodes);
  bytesTransmitted = std::vector<uint64_t> (numNodes);
  nAssociatedStasPerBss = std::vector<uint32_t> (nBss * n);

  busyTime = std::vector<Time> (nBss);
  for (uint32_t bss = 1; bss <= nBss; bss++)
    {
      busyTime[bss - 1] = Seconds (0);
    }

  packetsReceivedPerNode.resize (numNodes, std::vector<uint64_t> (numNodes, 0));
  rssiPerNode.resize (numNodes, std::vector<double> (numNodes, 0.0));

  for (uint32_t nodeId = 0; nodeId < numNodes; nodeId++)
    {
      packetsReceived[nodeId] = 0;
      bytesReceived[nodeId] = 0;
bytesTransmitted[nodeId] = 0;
    }

  // When logging, use prefixes
  LogComponentEnableAll (LOG_PREFIX_TIME);
  LogComponentEnableAll (LOG_PREFIX_FUNC);
  LogComponentEnableAll (LOG_PREFIX_NODE);

  PacketMetadata::Enable ();

  // Create nodes and containers
  Ptr<Node> ap1 = CreateObject<Node> ();
  Ptr<Node> ap2 = 0;
  Ptr<Node> ap3 = 0;
  Ptr<Node> ap4 = 0;
  Ptr<Node> ap5 = 0;
  Ptr<Node> ap6 = 0;
  Ptr<Node> ap7 = 0;
  // node containers for two APs and their STAs
  NodeContainer stasA, nodesA;
  NodeContainer stasB, nodesB;
  NodeContainer stasC, nodesC;
  NodeContainer stasD, nodesD;
  NodeContainer stasE, nodesE;
  NodeContainer stasF, nodesF;
  NodeContainer stasG, nodesG;

  FlowMonitorHelper flowmonHelperA;

  // network "A"
  for (uint32_t i = 0; i < n; i++)
    {
      Ptr<Node> sta = CreateObject<Node> ();
      stasA.Add (sta);
    }

  // AP at front of node container, then STAs
  nodesA.Add (ap1);
  nodesA.Add (stasA);

  if (nBss >= 2)
    {
      ap2 = CreateObject<Node> ();
      // network "B"
      for (uint32_t i = 0; i < n; i++)
        {
          Ptr<Node> sta = CreateObject<Node> ();
          stasB.Add (sta);
        }

      // AP at front of node container, then STAs
      nodesB.Add (ap2);
      nodesB.Add (stasB);
    }

  if (nBss >= 3)
    {
      ap3 = CreateObject<Node> ();
      // network "C"
      for (uint32_t i = 0; i < n; i++)
        {
          Ptr<Node> sta = CreateObject<Node> ();
          stasC.Add (sta);
        }

      // AP at front of node container, then STAs
      nodesC.Add (ap3);
      nodesC.Add (stasC);
    }

  if (nBss >= 4)
    {
      ap4 = CreateObject<Node> ();
      // network "D"
      for (uint32_t i = 0; i < n; i++)
        {
          Ptr<Node> sta = CreateObject<Node> ();
          stasD.Add (sta);
        }

      // AP at front of node container, then STAs
      nodesD.Add (ap4);
      nodesD.Add (stasD);
    }

  if (nBss >= 5)
    {
      ap5 = CreateObject<Node> ();
      // network "E"
      for (uint32_t i = 0; i < n; i++)
        {
          Ptr<Node> sta = CreateObject<Node> ();
          stasE.Add (sta);
        }

      // AP at front of node container, then STAs
      nodesE.Add (ap5);
      nodesE.Add (stasE);

    }
  
  if (nBss >= 6)
    {
      ap6 = CreateObject<Node> ();
      // network "F"
      for (uint32_t i = 0; i < n; i++)
        {
          Ptr<Node> sta = CreateObject<Node> ();
          stasF.Add (sta);
        }

      // AP at front of node container, then STAs
      nodesF.Add (ap6);
      nodesF.Add (stasF);
    }
  
  if (nBss >= 7)
    {
      ap7 = CreateObject<Node> ();
      // network "G"
      for (uint32_t i = 0; i < n; i++)
        {
          Ptr<Node> sta = CreateObject<Node> ();
          stasG.Add (sta);
        }

      // AP at front of node container, then STAs
      nodesG.Add (ap7);
      nodesG.Add (stasG);
    }

  // the container for all nodes (from Network "A" and Network "B")
  allNodes = NodeContainer (nodesA, nodesB, nodesC, nodesD);
  allNodes.Add(nodesE);
  allNodes.Add(nodesF);
  allNodes.Add(nodesG);

  // PHY setup
///////////////////////////////////////////////////////////////Pass Loss Modelの設定（ここから）///////////////////////////////////////////////////////////////////////////
  //SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();
  SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper(); //PHYのパラメータを保存する値を定義
  Ptr<MultiModelSpectrumChannel> spectrumChannel
    = CreateObject<MultiModelSpectrumChannel> ();
  // path loss model uses one of the 802.11ax path loss models
  // described in the TGax simulations scenarios.
  // currently using just the IndoorPropagationLossModel, which
  // appears suitable for Test2 - Enterprise.
  // additional code tweaks needed for Test 1 and Test 3,
  // handling of 'W=1 wall' and using the ItuUmitPropagationLossModel
  // for Test 4.
  //uint64_t lossModelStream = 500;
  if (scenario == "logdistance")
    {
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
     // more prominent example values:
      lossModel ->SetAttribute ("ReferenceDistance", DoubleValue (1)); //参照距離
      lossModel ->SetAttribute ("Exponent", DoubleValue (3.5)); //損失係数
      lossModel ->SetAttribute ("ReferenceLoss", DoubleValue (50)); //参照ロス
      spectrumChannel->AddPropagationLossModel (lossModel);   
    }
  // else if (scenario == "residential")
  //   {
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::DistanceBreakpoint", DoubleValue (5.0));
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::Walls", DoubleValue (1.0));
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::WallsFactor", DoubleValue (5.0));

  //     Ptr<Ieee80211axIndoorPropagationLossModel> lossModel = CreateObject<Ieee80211axIndoorPropagationLossModel> ();
  //     lossModel->AssignStreams (lossModelStream);
  //     spectrumChannel->AddPropagationLossModel (lossModel);
  //   }
  // else if (scenario == "enterprise")
  //   {
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::DistanceBreakpoint", DoubleValue (10.0));
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::Walls", DoubleValue (1.0));
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::WallsFactor", DoubleValue (7.0));

  //     Ptr<Ieee80211axIndoorPropagationLossModel> lossModel = CreateObject<Ieee80211axIndoorPropagationLossModel> ();
  //     lossModel->AssignStreams (lossModelStream);
  //     spectrumChannel->AddPropagationLossModel (lossModel);
  //   }
  // else if (scenario == "indoor")
  //   {
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::DistanceBreakpoint", DoubleValue (10.0));
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::Walls", DoubleValue (0.0));
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::WallsFactor", DoubleValue (0.0));
  //     Config::SetDefault ("ns3::Ieee80211axIndoorPropagationLossModel::Sigma", DoubleValue (sigma));

  //     Ptr<Ieee80211axIndoorPropagationLossModel> lossModel = CreateObject<Ieee80211axIndoorPropagationLossModel> ();
  //     lossModel->AssignStreams (lossModelStream);
  //     spectrumChannel->AddPropagationLossModel (lossModel);
  //   }
  // else if (scenario == "outdoor")
  //   {
  //     Ptr<ItuUmiPropagationLossModel> lossModel = CreateObject<ItuUmiPropagationLossModel> ();
  //     lossModel->AssignStreams (lossModelStream);
  //     spectrumChannel->AddPropagationLossModel (lossModel);
  //   }
  else
    {
      std::cout << "Unknown scenario: " << scenario << ". Must be one of:  residential, enterprise, indoor or outdoor." << std::endl;
      return 1;
    }

  Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
  spectrumChannel->SetPropagationDelayModel (delayModel);

////////////////////////////////////////////パケットの衝突などのモデルの設定（ここから）////////////////////////////////////////////////////////////////////////////////////////////////////////////
  spectrumPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO); //無線規格の設定

  spectrumPhy.SetChannel (spectrumChannel);
  spectrumPhy.SetErrorRateModel ("ns3::YansErrorRateModel"); //YANS is more aligned with link simulations

//衝突モデル（CCA_CS）は各BSSで設定するように変更

/*if (enableFrameCapture)
    {
      spectrumPhy.SetFrameCaptureModel ("ns3::SimpleFrameCaptureModel"); //Frame Captureありのモデルを設定
    }
  if (enableThresholdPreambleDetection)  //defaultでTrue
    {
      double CCA_CS_THR = -82; //CCA_CS_Threshold
      spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel","Threshold",DoubleValue(CCA_CS_THR+94)); 
      //preamble detectionありのモデルの設定(この関数がCS/CCAを決定する：threshold-preamble-detection-model.cc)
      //spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel"); //default
    }
*/


  spectrumPhy.Set ("Frequency", UintegerValue (freq)); // channel 36 at 20 MHz 周波数の設定
  spectrumPhy.Set ("ChannelWidth", UintegerValue (bw)); //帯域幅の設定
/*
  // WiFi setup / helpers → 各BSSごとにMACパラメータの定義を行うように変更
////////////////////////////////////////////////////////////////////MCSからデータレートを設定（ここから）/////////////////////////////////////////////////////////////////////////////////////////
  WifiMacHelper mac;

  if (useIdealWifiManager == false) //spatial-reuseではuseIdealWifiManager == falseでシミュレート
    {
      StringValue ctrlRate; //default
      if ((standard == "11n_2_4GHZ") || (standard == "11ax_2_4GHZ"))
        {
          if (mcs == 0)
            {
              ctrlRate = StringValue ("ErpOfdmRate6Mbps");
            }
          else if (mcs < 3)
            {
              ctrlRate = StringValue ("ErpOfdmRate12Mbps");
            }
          else
            {
              ctrlRate = StringValue ("ErpOfdmRate24Mbps");
            }
        }
      else //11ac=5GHzなのでこっちが動作
        {
          if (mcs == 0)
            {
              ctrlRate = StringValue ("OfdmRate6Mbps");
            }
          else if (mcs < 3)
            {
              ctrlRate = StringValue ("OfdmRate12Mbps");
            }
          else
            {
              ctrlRate = StringValue ("OfdmRate24Mbps");
            }
        }
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (dataRate), //Data Rate/Control frame rateの設定
                                    "ControlMode", ctrlRate); //Multi-BSS scenario (BSS1)
    }
  else
    {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
    }
*/
////////////////////////////////////////////////////////////////////CCA-ED、PHYのパワーを設定（ここから）/////////////////////////////////////////////////////////////////////////////////////////
  // Set PHY power and CCA threshold for STAs （BSS1のSTAのパラメータの設定）
  spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta)); //STAの送信開始時パワーの設定
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta)); //STAの送信終了時のパワーの設定
  spectrumPhy.Set ("TxGain", DoubleValue (txGain)); //TXのゲインの設定
  spectrumPhy.Set ("RxGain", DoubleValue (rxGain)); //RXのゲインの設定
  spectrumPhy.Set ("Antennas", UintegerValue (antennas)); //アンテナ数の設定
  spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams)); //送信に使用するアンテナ数の最大数
  spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams)); //受信に使用するアンテナの最大数
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrSta1)); //Multi-BSS scenario (BSS1) CCA-ED閾値の設定
  spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity)); 
  std::cout << "BSS1_CCA-ED_Threshold(STA) = " << ccaTrSta1 << std::endl;
  // Network "A"
  Ssid ssidA = Ssid ("A");
  if (enableObssPd) //spatial-reuseではenableObssPd=falseなので実行されない
    {
      wifi.SetObssPdAlgorithm ("ns3::"+obssPdAlgorithm,
                               "ObssPdLevelMin", DoubleValue (obssPdThresholdMinBss1),
                               "ObssPdLevelMax", DoubleValue (obssPdThresholdMaxBss1),
                               "ObssPdLevel", DoubleValue (obssPdThresholdBss1),
                               "d",DoubleValue (d),
                                 "r",DoubleValue (r1), //Multi-BSS scenario (BSS1)
                                "mcs",UintegerValue(mcs1)); //Multi-BSS scenario (BSS1)
    }
///////////////////////////////////////BSS1のCCA_CSの設定//////////////////////////////////////////////////////////////////
if (enableFrameCapture)
    {
      spectrumPhy.SetFrameCaptureModel ("ns3::SimpleFrameCaptureModel"); //Frame Captureありのモデルを設定
    }
  if (enableThresholdPreambleDetection)  //defaultでTrue
    {
      double CCA_CS_THR_BSS1 = -82; //CCA_CS_Threshold
      std::cout << "BSS1_CCA_CS_Threshold = " << CCA_CS_THR_BSS1 << std::endl;
      spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel","Threshold",DoubleValue(CCA_CS_THR_BSS1+94)); 
      //preamble detectionありのモデルの設定(この関数がCS/CCAを決定する：threshold-preamble-detection-model.cc)
      //spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel"); //default
    }
////////////////////////////////////BSS1のMACパラメータの設定////////////////////////////////////////////////////////////////////
 WifiMacHelper mac;

  if (useIdealWifiManager == false) //spatial-reuseではuseIdealWifiManager == falseでシミュレート
    {
      StringValue ctrlRate; //default
      StringValue ctrlRate1; //Multi-BSS scenario (BSS1)
      if ((standard == "11n_2_4GHZ") || (standard == "11ax_2_4GHZ"))
        {
          if (mcs1 == 0)
            {
              ctrlRate1 = StringValue ("ErpOfdmRate6Mbps");
            }
          else if (mcs1 < 3)
            {
              ctrlRate1 = StringValue ("ErpOfdmRate12Mbps");
            }
          else
            {
              ctrlRate1 = StringValue ("ErpOfdmRate24Mbps");
            }
        }
      else //11ac=5GHzなのでこっちが動作
        {
          if (mcs1 == 0)
            {
              ctrlRate1 = StringValue ("OfdmRate6Mbps");
            }
          else if (mcs1 < 3)
            {
              ctrlRate1 = StringValue ("OfdmRate12Mbps");
            }
          else
            {
              ctrlRate1 = StringValue ("OfdmRate24Mbps");
            }
        }
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (dataRate1), //Data Rate/Control frame rateの設定
                                    "ControlMode", ctrlRate1); //Multi-BSS scenario (BSS1)
    }
  else
    {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
    }
std::cout << "BSS1_MCS = " << mcs1 << std::endl;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  mac.SetType ("ns3::StaWifiMac",
               "MaxMissedBeacons", UintegerValue (maxMissedBeacons),
               "Ssid", SsidValue (ssidA));

  uint64_t wifiStream = 700;

  staDevicesA = wifi.Install (spectrumPhy, mac, stasA); //STA(BSS1)のPHY、MACパラメータ、ノード数を設定
  wifi.AssignStreams (staDevicesA, wifiStream + 0);

  // Set  PHY power and CCA threshold for APs （BSS1のAPのパラメータの設定）
  spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
  spectrumPhy.Set ("TxGain", DoubleValue (txGain));
  spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
  spectrumPhy.Set ("Antennas", UintegerValue (antennas));
  spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
  spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrAp1)); //Multi-BSS scenario (BSS1)
  spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssidA));
  
 
  // AP1
  apDeviceA = wifi.Install (spectrumPhy, mac, ap1);
  wifi.AssignStreams (apDeviceA, wifiStream + 1);
  Ptr<WifiNetDevice> apDevice = apDeviceA.Get (0)->GetObject<WifiNetDevice> ();
  Ptr<ApWifiMac> apWifiMac = apDevice->GetMac ()->GetObject<ApWifiMac> ();

  // The below statements may be simplified in a future HeConfigurationHelper
  if ((enableObssPd))
    {
      Ptr<HeConfiguration> heConfiguration = apDevice->GetHeConfiguration ();
      heConfiguration->SetAttribute ("BssColor", UintegerValue (colorBss1));
    }
////////////////////////////////////////////////////////////////////BSS2の設定///////////////////////////////////////////////////////////////////////
  if (nBss >= 2)
    {
      // Set PHY power and CCA threshold for STAs
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrSta2)); //Multi-BSS scenario (BSS2)
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));
      std::cout << "BSS2_CCA-ED_Threshold(STA) = " << ccaTrSta2 << std::endl;
      // Network "B"
      Ssid ssidB = Ssid ("B");
      if (enableObssPd)
        {
          wifi.SetObssPdAlgorithm ("ns3::"+obssPdAlgorithm,
                                   "ObssPdLevelMin", DoubleValue (obssPdThresholdMinBss2),
                                   "ObssPdLevelMax", DoubleValue (obssPdThresholdMaxBss2),
                                   "ObssPdLevel", DoubleValue (obssPdThresholdBss2),
                               "d",DoubleValue (d),
                                 "r",DoubleValue (r2), //Multi-BSS scenario (BSS2)
                                "mcs",UintegerValue(mcs2)); //Multi-BSS scenario (BSS2)
        }
///////////////////////////////////////BSS2のCCA_CSの設定//////////////////////////////////////////////////////////////////
if (enableFrameCapture)
    {
      spectrumPhy.SetFrameCaptureModel ("ns3::SimpleFrameCaptureModel"); //Frame Captureありのモデルを設定
    }
  if (enableThresholdPreambleDetection)  //defaultでTrue
    {
      double CCA_CS_THR_BSS2 = -82; //CCA_CS_Threshold
      std::cout << "BSS2_CCA_CS_Threshold = " << CCA_CS_THR_BSS2 << std::endl;
      spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel","Threshold",DoubleValue(CCA_CS_THR_BSS2+94)); 
      //preamble detectionありのモデルの設定(この関数がCS/CCAを決定する：threshold-preamble-detection-model.cc)
      //spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel"); //default
    }
////////////////////////////////////BSS2のMACパラメータの設定////////////////////////////////////////////////////////////////////
 WifiMacHelper mac;

  if (useIdealWifiManager == false) //spatial-reuseではuseIdealWifiManager == falseでシミュレート
    {
      StringValue ctrlRate; //default
      StringValue ctrlRate2; //Multi-BSS scenario (BSS1)
      if ((standard == "11n_2_4GHZ") || (standard == "11ax_2_4GHZ"))
        {
          if (mcs2 == 0)
            {
              ctrlRate2 = StringValue ("ErpOfdmRate6Mbps");
            }
          else if (mcs2 < 3)
            {
              ctrlRate2 = StringValue ("ErpOfdmRate12Mbps");
            }
          else
            {
              ctrlRate2 = StringValue ("ErpOfdmRate24Mbps");
            }
        }
      else //11ac=5GHzなのでこっちが動作
        {
          if (mcs2 == 0)
            {
              ctrlRate2 = StringValue ("OfdmRate6Mbps");
            }
          else if (mcs2 < 3)
            {
              ctrlRate2 = StringValue ("OfdmRate12Mbps");
            }
          else
            {
              ctrlRate2 = StringValue ("OfdmRate24Mbps");
            }
        }
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (dataRate2), //Data Rate/Control frame rateの設定
                                    "ControlMode", ctrlRate2); //Multi-BSS scenario (BSS2)
    }
  else
    {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
    }
std::cout << "BSS2_MCS = " << mcs2 << std::endl;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      mac.SetType ("ns3::StaWifiMac",
                   "MaxMissedBeacons", UintegerValue (maxMissedBeacons),
                   "Ssid", SsidValue (ssidB));

      staDevicesB = wifi.Install (spectrumPhy, mac, stasB);
      wifi.AssignStreams (staDevicesB, wifiStream + 2);

      // Set PHY power and CCA threshold for AP
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrAp2)); //Multi-BSS scenario (BSS2)
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssidB));

      // AP2
      apDeviceB = wifi.Install (spectrumPhy, mac, ap2);
      wifi.AssignStreams (apDeviceB, wifiStream + 3);
      Ptr<WifiNetDevice> ap2Device = apDeviceB.Get (0)->GetObject<WifiNetDevice> ();
      apWifiMac = ap2Device->GetMac ()->GetObject<ApWifiMac> ();
      if (enableObssPd)
        {
          Ptr <HeConfiguration> heConfiguration = ap2Device->GetHeConfiguration ();
          heConfiguration->SetAttribute ("BssColor", UintegerValue (colorBss2));
        }
    }

  if (nBss >= 3)
    {
      // Set PHY power and CCA threshold for STAs
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrSta3)); 
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));
      std::cout << "CCA-ED_Threshold(BSS3_STA) = " << ccaTrSta3 << std::endl;
      // Network "C"
      Ssid ssidC = Ssid ("C");
      if (enableObssPd)
        {
          wifi.SetObssPdAlgorithm ("ns3::"+obssPdAlgorithm,
                                   "ObssPdLevelMin", DoubleValue (obssPdThresholdMinBss3),
                                   "ObssPdLevelMax", DoubleValue (obssPdThresholdMaxBss3),
                                   "ObssPdLevel", DoubleValue (obssPdThresholdBss3),
                               "d",DoubleValue (d),
                                 "r",DoubleValue (r),
                                "mcs",UintegerValue(mcs3));
        }
///////////////////////////////////////BSS3のCCA_CSの設定//////////////////////////////////////////////////////////////////
if (enableFrameCapture)
    {
      spectrumPhy.SetFrameCaptureModel ("ns3::SimpleFrameCaptureModel"); //Frame Captureありのモデルを設定
    }
  if (enableThresholdPreambleDetection)  //defaultでTrue
    {
      double CCA_CS_THR_BSS3 = -82; //CCA_CS_Threshold
      std::cout << "BSS3_CCA_CS_Threshold = " << CCA_CS_THR_BSS3 << std::endl;
      spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel","Threshold",DoubleValue(CCA_CS_THR_BSS3+94)); 
      //preamble detectionありのモデルの設定(この関数がCS/CCAを決定する：threshold-preamble-detection-model.cc)
      //spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel"); //default
    }
////////////////////////////////////BSS3のMACパラメータの設定////////////////////////////////////////////////////////////////////
 WifiMacHelper mac;

  if (useIdealWifiManager == false) //spatial-reuseではuseIdealWifiManager == falseでシミュレート
    {
      StringValue ctrlRate; //default
      StringValue ctrlRate3; //Multi-BSS scenario (BSS3)
      if ((standard == "11n_2_4GHZ") || (standard == "11ax_2_4GHZ"))
        {
          if (mcs3 == 0)
            {
              ctrlRate3 = StringValue ("ErpOfdmRate6Mbps");
            }
          else if (mcs3 < 3)
            {
              ctrlRate3 = StringValue ("ErpOfdmRate12Mbps");
            }
          else
            {
              ctrlRate3 = StringValue ("ErpOfdmRate24Mbps");
            }
        }
      else //11ac=5GHzなのでこっちが動作
        {
          if (mcs3 == 0)
            {
              ctrlRate3 = StringValue ("OfdmRate6Mbps");
            }
          else if (mcs3 < 3)
            {
              ctrlRate3 = StringValue ("OfdmRate12Mbps");
            }
          else
            {
              ctrlRate3 = StringValue ("OfdmRate24Mbps");
            }
        }
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (dataRate3), //Data Rate/Control frame rateの設定
                                    "ControlMode", ctrlRate3); //Multi-BSS scenario (BSS1)
    }
  else
    {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
    }
std::cout << "BSS3_MCS = " << mcs3 << std::endl;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      mac.SetType ("ns3::StaWifiMac",
                   "MaxMissedBeacons", UintegerValue (maxMissedBeacons),
                   "Ssid", SsidValue (ssidC));

      staDevicesC = wifi.Install (spectrumPhy, mac, stasC);
      wifi.AssignStreams (staDevicesC, wifiStream + 4);

      // Set PHY power and CCA threshold for AP
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrAp3));
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssidC));

      // AP3
      apDeviceC = wifi.Install (spectrumPhy, mac, ap3);
      wifi.AssignStreams (apDeviceC, wifiStream + 5);
      Ptr<WifiNetDevice> ap3Device = apDeviceC.Get (0)->GetObject<WifiNetDevice> ();
      apWifiMac = ap3Device->GetMac ()->GetObject<ApWifiMac> ();
      if (enableObssPd)
        {
          Ptr <HeConfiguration> heConfiguration = ap3Device->GetHeConfiguration ();
          heConfiguration->SetAttribute ("BssColor", UintegerValue (colorBss3));
        }
    }

  if (nBss >= 4)
    {
      // Set PHY power and CCA threshold for STAs
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrSta4));
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));
      std::cout << "CCA-ED_Threshold(BSS4_STA) = " << ccaTrSta4 << std::endl;
      // Network "D"
      Ssid ssidD = Ssid ("D");
      if (enableObssPd)
        {
          wifi.SetObssPdAlgorithm ("ns3::"+obssPdAlgorithm,
                                   "ObssPdLevelMin", DoubleValue (obssPdThresholdMinBss4),
                                   "ObssPdLevelMax", DoubleValue (obssPdThresholdMaxBss4),
                                   "ObssPdLevel", DoubleValue (obssPdThresholdBss4),
                               "d",DoubleValue (d),
                                 "r",DoubleValue (r),
                                "mcs",UintegerValue(mcs4));
        }
///////////////////////////////////////BSS4のCCA_CSの設定//////////////////////////////////////////////////////////////////
if (enableFrameCapture)
    {
      spectrumPhy.SetFrameCaptureModel ("ns3::SimpleFrameCaptureModel"); //Frame Captureありのモデルを設定
    }
  if (enableThresholdPreambleDetection)  //defaultでTrue
    {
      double CCA_CS_THR_BSS4 = -82; //CCA_CS_Threshold
      std::cout << "BSS4_CCA_CS_Threshold = " << CCA_CS_THR_BSS4 << std::endl;
      spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel","Threshold",DoubleValue(CCA_CS_THR_BSS4+94)); 
      //preamble detectionありのモデルの設定(この関数がCS/CCAを決定する：threshold-preamble-detection-model.cc)
      //spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel"); //default
    }
////////////////////////////////////BSS4のMACパラメータの設定////////////////////////////////////////////////////////////////////
 WifiMacHelper mac;

  if (useIdealWifiManager == false) //spatial-reuseではuseIdealWifiManager == falseでシミュレート
    {
      StringValue ctrlRate; //default
      StringValue ctrlRate4; //Multi-BSS scenario (BSS1)
      if ((standard == "11n_2_4GHZ") || (standard == "11ax_2_4GHZ"))
        {
          if (mcs4 == 0)
            {
              ctrlRate4 = StringValue ("ErpOfdmRate6Mbps");
            }
          else if (mcs4 < 3)
            {
              ctrlRate4 = StringValue ("ErpOfdmRate12Mbps");
            }
          else
            {
              ctrlRate4 = StringValue ("ErpOfdmRate24Mbps");
            }
        }
      else //11ac=5GHzなのでこっちが動作
        {
          if (mcs4 == 0)
            {
              ctrlRate4 = StringValue ("OfdmRate6Mbps");
            }
          else if (mcs4 < 3)
            {
              ctrlRate4 = StringValue ("OfdmRate12Mbps");
            }
          else
            {
              ctrlRate4 = StringValue ("OfdmRate24Mbps");
            }
        }
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (dataRate4), //Data Rate/Control frame rateの設定
                                    "ControlMode", ctrlRate4); //Multi-BSS scenario (BSS4)
    }
  else
    {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
    }
std::cout << "BSS4_MCS = " << mcs4 << std::endl;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      mac.SetType ("ns3::StaWifiMac",
                   "MaxMissedBeacons", UintegerValue (maxMissedBeacons),
                   "Ssid", SsidValue (ssidD));

      staDevicesD = wifi.Install (spectrumPhy, mac, stasD);
      wifi.AssignStreams (staDevicesD, wifiStream + 6);

      // Set PHY power and CCA threshold for AP
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrAp4));
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssidD));

      // AP4
      apDeviceD = wifi.Install (spectrumPhy, mac, ap4);
      wifi.AssignStreams (apDeviceD, wifiStream + 7);
      Ptr<WifiNetDevice> ap4Device = apDeviceD.Get (0)->GetObject<WifiNetDevice> ();
      apWifiMac = ap4Device->GetMac ()->GetObject<ApWifiMac> ();
      if (enableObssPd)
        {
          Ptr <HeConfiguration> heConfiguration = ap4Device->GetHeConfiguration ();
          heConfiguration->SetAttribute ("BssColor", UintegerValue (colorBss4));
        }
    }
/////////////////////////////////////////BSS5の設定/////////////////////////////////////////////////////////////////////
  if (nBss >= 5)
    {
      // Set PHY power and CCA threshold for STAs
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrSta5));
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));
      std::cout << "CCA-ED_Threshold(BSS5_STA) = " << ccaTrSta5 << std::endl;
      // Network "E"
      Ssid ssidE = Ssid ("E");
      if (enableObssPd)
        {
          wifi.SetObssPdAlgorithm ("ns3::"+obssPdAlgorithm,
                                   "ObssPdLevelMin", DoubleValue (obssPdThresholdMinBss5),
                                   "ObssPdLevelMax", DoubleValue (obssPdThresholdMaxBss5),
                                   "ObssPdLevel", DoubleValue (obssPdThresholdBss5),
                               "d",DoubleValue (d),
                                 "r",DoubleValue (r),
                                "mcs",UintegerValue(mcs5));
        }
///////////////////////////////////////BSS5のCCA_CSの設定//////////////////////////////////////////////////////////////////
if (enableFrameCapture)
    {
      spectrumPhy.SetFrameCaptureModel ("ns3::SimpleFrameCaptureModel"); //Frame Captureありのモデルを設定
    }
  if (enableThresholdPreambleDetection)  //defaultでTrue
    {
      double CCA_CS_THR_BSS5 = -82; //CCA_CS_Threshold
      std::cout << "BSS3_CCA_CS_Threshold = " << CCA_CS_THR_BSS5 << std::endl;
      spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel","Threshold",DoubleValue(CCA_CS_THR_BSS5+94)); 
      //preamble detectionありのモデルの設定(この関数がCS/CCAを決定する：threshold-preamble-detection-model.cc)
      //spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel"); //default
    }
////////////////////////////////////BSS5のMACパラメータの設定////////////////////////////////////////////////////////////////////
 WifiMacHelper mac;

  if (useIdealWifiManager == false) //spatial-reuseではuseIdealWifiManager == falseでシミュレート
    {
      StringValue ctrlRate; //default
      StringValue ctrlRate5; //Multi-BSS scenario (BSS5)
      if ((standard == "11n_2_4GHZ") || (standard == "11ax_2_4GHZ"))
        {
          if (mcs5 == 0)
            {
              ctrlRate5 = StringValue ("ErpOfdmRate6Mbps");
            }
          else if (mcs5 < 3)
            {
              ctrlRate5 = StringValue ("ErpOfdmRate12Mbps");
            }
          else
            {
              ctrlRate5 = StringValue ("ErpOfdmRate24Mbps");
            }
        }
      else //11ac=5GHzなのでこっちが動作
        {
          if (mcs5 == 0)
            {
              ctrlRate5 = StringValue ("OfdmRate6Mbps");
            }
          else if (mcs5 < 3)
            {
              ctrlRate5 = StringValue ("OfdmRate12Mbps");
            }
          else
            {
              ctrlRate5 = StringValue ("OfdmRate24Mbps");
            }
        }
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (dataRate5), //Data Rate/Control frame rateの設定
                                    "ControlMode", ctrlRate5); //Multi-BSS scenario (BSS5)
    }
  else
    {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
    }
std::cout << "BSS5_MCS = " << mcs5 << std::endl;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      mac.SetType ("ns3::StaWifiMac",
                   "MaxMissedBeacons", UintegerValue (maxMissedBeacons),
                   "Ssid", SsidValue (ssidE));

      staDevicesE = wifi.Install (spectrumPhy, mac, stasE);
      wifi.AssignStreams (staDevicesE, wifiStream + 8);

      // Set PHY power and CCA threshold for AP
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrAp5));
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssidE));

      // AP5
      apDeviceE = wifi.Install (spectrumPhy, mac, ap5);
      wifi.AssignStreams (apDeviceE, wifiStream + 9);
      Ptr<WifiNetDevice> ap5Device = apDeviceE.Get (0)->GetObject<WifiNetDevice> ();
      apWifiMac = ap5Device->GetMac ()->GetObject<ApWifiMac> ();
      if (enableObssPd)
        {
          Ptr <HeConfiguration> heConfiguration = ap5Device->GetHeConfiguration ();
          heConfiguration->SetAttribute ("BssColor", UintegerValue (colorBss5));
        }
    }

  if (nBss >= 6)
    {
      // Set PHY power and CCA threshold for STAs
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrSta6));
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));
      std::cout << "CCA-ED_Threshold(BSS6_STA) = " << ccaTrSta6 << std::endl;
      // Network "F"
      Ssid ssidF = Ssid ("F");
      if (enableObssPd)
        {
          wifi.SetObssPdAlgorithm ("ns3::"+obssPdAlgorithm,
                                   "ObssPdLevelMin", DoubleValue (obssPdThresholdMinBss6),
                                   "ObssPdLevelMax", DoubleValue (obssPdThresholdMaxBss6),
                                   "ObssPdLevel", DoubleValue (obssPdThresholdBss6),
                               "d",DoubleValue (d),
                                 "r",DoubleValue (r),
                                "mcs",UintegerValue(mcs6));
        }
///////////////////////////////////////BSS6のCCA_CSの設定//////////////////////////////////////////////////////////////////
if (enableFrameCapture)
    {
      spectrumPhy.SetFrameCaptureModel ("ns3::SimpleFrameCaptureModel"); //Frame Captureありのモデルを設定
    }
  if (enableThresholdPreambleDetection)  //defaultでTrue
    {
      double CCA_CS_THR_BSS6 = -82; //CCA_CS_Threshold
      std::cout << "BSS6_CCA_CS_Threshold = " << CCA_CS_THR_BSS6 << std::endl;
      spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel","Threshold",DoubleValue(CCA_CS_THR_BSS6+94)); 
      //preamble detectionありのモデルの設定(この関数がCS/CCAを決定する：threshold-preamble-detection-model.cc)
      //spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel"); //default
    }
////////////////////////////////////BSS6のMACパラメータの設定////////////////////////////////////////////////////////////////////
 WifiMacHelper mac;

  if (useIdealWifiManager == false) //spatial-reuseではuseIdealWifiManager == falseでシミュレート
    {
      StringValue ctrlRate; //default
      StringValue ctrlRate6; //Multi-BSS scenario (BSS6)
      if ((standard == "11n_2_4GHZ") || (standard == "11ax_2_4GHZ"))
        {
          if (mcs6 == 0)
            {
              ctrlRate6 = StringValue ("ErpOfdmRate6Mbps");
            }
          else if (mcs6 < 3)
            {
              ctrlRate6 = StringValue ("ErpOfdmRate12Mbps");
            }
          else
            {
              ctrlRate6 = StringValue ("ErpOfdmRate24Mbps");
            }
        }
      else //11ac=5GHzなのでこっちが動作
        {
          if (mcs6 == 0)
            {
              ctrlRate6 = StringValue ("OfdmRate6Mbps");
            }
          else if (mcs6 < 3)
            {
              ctrlRate6 = StringValue ("OfdmRate12Mbps");
            }
          else
            {
              ctrlRate6 = StringValue ("OfdmRate24Mbps");
            }
        }
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (dataRate6), //Data Rate/Control frame rateの設定
                                    "ControlMode", ctrlRate6); //Multi-BSS scenario (BSS6)
    }
  else
    {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
    }
std::cout << "BSS6_MCS = " << mcs6 << std::endl;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      mac.SetType ("ns3::StaWifiMac",
                   "MaxMissedBeacons", UintegerValue (maxMissedBeacons),
                   "Ssid", SsidValue (ssidF));

      staDevicesF = wifi.Install (spectrumPhy, mac, stasF);
      wifi.AssignStreams (staDevicesF, wifiStream + 10);

      // Set PHY power and CCA threshold for AP
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrAp));//使わない(ccaTrAp)
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssidF));

      // AP6
      apDeviceF = wifi.Install (spectrumPhy, mac, ap6);
      wifi.AssignStreams (apDeviceF, wifiStream + 11);
      Ptr<WifiNetDevice> ap6Device = apDeviceF.Get (0)->GetObject<WifiNetDevice> ();
      apWifiMac = ap6Device->GetMac ()->GetObject<ApWifiMac> ();
      if (enableObssPd)
        {
          Ptr <HeConfiguration> heConfiguration = ap6Device->GetHeConfiguration ();
          heConfiguration->SetAttribute ("BssColor", UintegerValue (colorBss6));
        }
    }

  if (nBss >= 7)
  {
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrAp));//使わない(ccaTrAp)
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));

      // Network "G"
      Ssid ssidG = Ssid ("G");
      if (enableObssPd)
        {
          wifi.SetObssPdAlgorithm ("ns3::"+obssPdAlgorithm,
                                   "ObssPdLevelMin", DoubleValue (obssPdThresholdMinBss7),
                                   "ObssPdLevelMax", DoubleValue (obssPdThresholdMaxBss7),
                                   "ObssPdLevel", DoubleValue (obssPdThresholdBss7),
                               "d",DoubleValue (d),
                                 "r",DoubleValue (r),
                                "mcs",UintegerValue(mcs));
        }

      mac.SetType ("ns3::StaWifiMac",
                   "MaxMissedBeacons", UintegerValue (maxMissedBeacons),
                   "Ssid", SsidValue (ssidG));
    
      staDevicesG = wifi.Install (spectrumPhy, mac, stasG);
      wifi.AssignStreams (staDevicesG, wifiStream + 12);
    
      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssidG));

      // AP7
      apDeviceG = wifi.Install (spectrumPhy, mac, ap7);
      wifi.AssignStreams (apDeviceG, wifiStream + 13);
      Ptr<WifiNetDevice> ap7Device = apDeviceG.Get (0)->GetObject<WifiNetDevice> ();
      apWifiMac = ap7Device->GetMac ()->GetObject<ApWifiMac> ();
      if (enableObssPd)
        {
          Ptr <HeConfiguration> heConfiguration = ap7Device->GetHeConfiguration ();
          heConfiguration->SetAttribute ("BssColor", UintegerValue (colorBss7));
        }
    }

  // Assign positions to all nodes using position allocator
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  // allocate in the order of AP_A, STAs_A, AP_B, STAs_B
  std::string filename = outputFilePrefix + "-positions-" + testname + ".csv";
  std::ofstream positionOutFile;
  positionOutFile.open (filename.c_str (), std::ofstream::out | std::ofstream::trunc);
  positionOutFile.setf (std::ios_base::fixed);
  positionOutFile.flush ();

  if (!positionOutFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return 1;
    }

  // bounding box
  if ((nBss != 7) || (scenario != "indoor"))
    {
      double boundingBoxExtension = 10.0;
      double dx = d;
      double dy = d;
      if (nBss == 1)
        {
          dx = 0;
          dy = 0;
        }
      else if (nBss == 2)
        {
          dy = 0;
        }
      positionOutFile << -(r + boundingBoxExtension) <<      ", " << -dy + -r - boundingBoxExtension << std::endl; //変更？
      positionOutFile << (dx + r + boundingBoxExtension) << ", " << -dy + -r - boundingBoxExtension << std::endl;
      positionOutFile << (dx + r + boundingBoxExtension) << ", " <<  r + boundingBoxExtension << std::endl;
      positionOutFile << -(r + boundingBoxExtension) <<      ", " <<  r + boundingBoxExtension << std::endl;
    }
  else
    {
      // study1 and study 2 scenario bounding box
      positionOutFile << -40.0 <<  ", " << -40.0 << std::endl;
      positionOutFile <<  40.0 <<  ", " << -40.0 << std::endl;
      positionOutFile <<  40.0 <<  ", " <<  40.0 << std::endl;
      positionOutFile <<  40.0 <<  ", " << -40.0 << std::endl;
    }
  positionOutFile << std::endl;
  positionOutFile << std::endl;

  // consistent stream to that positional layout is not perturbed by other configuration choices
  int64_t streamNumber = 100;

  if ((nodePositionsFile != "") && (nodePositionsFile != "NONE"))
    {
      std::cout << "Loading node positions from file: " << nodePositionsFile << std::endl;
      // Create Ns2MobilityHelper with the specified trace log file as parameter
      Ns2MobilityHelper ns2 = Ns2MobilityHelper (nodePositionsFile);
      ns2.Install (); // configure movements for each node, while reading trace file

      uint32_t numNodes = nodesA.GetN ();
      for (uint32_t nodeIdx = 0; nodeIdx < numNodes; nodeIdx++)
        {
          Ptr<Node> node = nodesA.Get (nodeIdx);
          Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
          Vector position = mob->GetPosition ();
          double x = position.x;
          double y = position.y;

          // nodeIdx == 0 is AP1
          if (nodeIdx == 0)
            {
              // "A" - APs
              positionOutFile << x << ", " << y << ", " << r << ", " << csr << std::endl;
              positionOutFile << std::endl;
              positionOutFile << std::endl;

              // "B" - APs
              // Position is not provided in Ns2Mobility file, but we have to output something
              // here so that the plotting script works.
              positionOutFile << 0.0 << ", " << 0.0 << ", " << r << ", " << csr << std::endl;
              positionOutFile << std::endl;
              positionOutFile << std::endl;
            }
          else
            {
              positionOutFile << x << ", " << y << std::endl;
            }
        }
      // End of nodes for BSS1.  Output blank lines as section separator.
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // need to output something here to represent the positions section for STAs B
      // since the post-processnig script expects there to be something here.
      positionOutFile << d << ", " << 0 << std::endl;
    }
  else if ((nBss == 7) && (scenario == "indoor")) //study 1 and 2
    {
      double x1 = 0.0;
      double y1 = 0.0;
      double theta = 0.0;  // radians
      double x2 = d * cos(theta);
      double y2 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x3 = d * cos(theta);
      double y3 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x4 = d * cos(theta);
      double y4 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x5 = d * cos(theta);
      double y5 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x6 = d * cos(theta);
      double y6 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x7 = d * cos(theta);
      double y7 = d * sin(theta);

      // "A" - APs
      positionOutFile << x1 << ", " << y1 << ", " << r << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // "B" - APs
      positionOutFile << x2 << ", " << y2 << ", " << r << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // "C" - APs
      positionOutFile << x3 << ", " << y3 << ", " << r << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // "D" - APs
      positionOutFile << x4 << ", " << y4 << ", " << r << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // "E" - APs
      positionOutFile << x5 << ", " << y5 << ", " << r << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // "F" - APs
      positionOutFile << x6 << ", " << y6 << ", " << r << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // "G" - APs
      positionOutFile << x7 << ", " << y7 << ", " << r << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // Network "A"
      // AP1
      positionAlloc->Add (Vector (x1, y1, 0.0));
      // STAs for AP1
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator1 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator1->AssignStreams (streamNumber);
      // AP1 is at origin (x=x1, y=y1), with radius Rho=r
      unitDiscPositionAllocator1->SetX (x1);
      unitDiscPositionAllocator1->SetY (y1);
      unitDiscPositionAllocator1->SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator1->GetNext ();
          positionOutFile << v.x << ", " << v.y << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // Network "B"
      // AP2
      positionAlloc->Add (Vector (x2, y2, 0.0));
      // STAs for AP2
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator2 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator2->AssignStreams (streamNumber + 1);
      // AP1 is at origin (x=x2, y=y2), with radius Rho=r
      unitDiscPositionAllocator2->SetX (x2);
      unitDiscPositionAllocator2->SetY (y2);
      unitDiscPositionAllocator2->SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator1->GetNext ();
          positionOutFile << v.x << ", " << v.y << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // Network "C"
      // AP3
      positionAlloc->Add (Vector (x3, y3, 0.0));
      // STAs for AP3
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator3 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator3->AssignStreams (streamNumber + 2);
      // AP1 is at origin (x=x3, y=y3), with radius Rho=r
      unitDiscPositionAllocator3->SetX (x3);
      unitDiscPositionAllocator3->SetY (y3);
      unitDiscPositionAllocator3->SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator3->GetNext ();
          positionAlloc->Add (v);
          positionOutFile << v.x << ", " << v.y << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // Network "D"
      // AP4
      positionAlloc->Add (Vector (x4, y4, 0.0));
      // STAs for AP4
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator4 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator4->AssignStreams (streamNumber + 3);
      // AP1 is at origin (x=x4, y=y4), with radius Rho=r
      unitDiscPositionAllocator4->SetX (x4);
      unitDiscPositionAllocator4->SetY (y4);
      unitDiscPositionAllocator4->SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator4->GetNext ();
          positionAlloc->Add (v);
          positionOutFile << v.x << ", " << v.y << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // Network "E"
      // AP5
      positionAlloc->Add (Vector (x5, y5, 0.0));
      // STAs for AP5
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator5 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator5->AssignStreams (streamNumber + 4);
      // AP1 is at origin (x=x5, y=y5), with radius Rho=r
      unitDiscPositionAllocator5->SetX (x5);
      unitDiscPositionAllocator5->SetY (y5);
      unitDiscPositionAllocator5->SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator5->GetNext ();
          positionAlloc->Add (v);
          positionOutFile << v.x << ", " << v.y << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // Network "F"
      // AP6
      positionAlloc->Add (Vector (x6, y6, 0.0));
      // STAs for AP6
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator6 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator6->AssignStreams (streamNumber + 5);
      // AP1 is at origin (x=x6, y=y6), with radius Rho=r
      unitDiscPositionAllocator6->SetX (x6);
      unitDiscPositionAllocator6->SetY (y6);
      unitDiscPositionAllocator6->SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator6->GetNext ();
          positionAlloc->Add (v);
          positionOutFile << v.x << ", " << v.y << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      // Network "G"
      // AP7
      positionAlloc->Add (Vector (x7, y7, 0.0));
      // STAs for AP7
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator7 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator7->AssignStreams (streamNumber + 6);
      // AP1 is at origin (x=x7, y=y7), with radius Rho=r
      unitDiscPositionAllocator7->SetX (x7);
      unitDiscPositionAllocator7->SetY (y7);
      unitDiscPositionAllocator7->SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator7->GetNext ();
          positionAlloc->Add (v);
          positionOutFile << v.x << ", " << v.y << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.SetPositionAllocator (positionAlloc);
      mobility.Install (allNodes);
    }
  else
    {
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//----------------------senario=logdistance-----------------------------------------------------------------------------
      double x1 = 0.0;
      double y1 = 0.0;
      double theta = 0.0;  // radians
      double x2 = d * cos(theta);
      double y2 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x3 = d * cos(theta);
      double y3 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x4 = d * cos(theta);
      double y4 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x5 = d * cos(theta);
      double y5 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x6 = d * cos(theta);
      double y6 = d * sin(theta);
      theta += 60.0 / 360.0 * 2.0 * M_PI;
      double x7 = d * cos(theta);
      double y7 = d * sin(theta);

      // "A" - APs
      positionOutFile << x1 << ", " << y1 << ", " << r1 << ", " << csr << std::endl; //Multi-BSS scenario (BSS1)
      positionOutFile << std::endl;
      positionOutFile << std::endl;
      if (nBss >= 2)
        {
      // "B" - APs
      positionOutFile << x2 << ", " << y2 << ", " << r2 << ", " << csr << std::endl; //Multi-BSS scenario (BSS2)
      positionOutFile << std::endl;
      positionOutFile << std::endl;
        }
      if (nBss >= 3)
        {
      // "C" - APs
      positionOutFile << x3 << ", " << y3 << ", " << r3 << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;
        }
      if (nBss >= 4)
        {
      // "D" - APs
      positionOutFile << x4 << ", " << y4 << ", " << r4 << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;
        }
      if (nBss >= 5)
        {
      // "E" - APs
      positionOutFile << x5 << ", " << y5 << ", " << r5 << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;
        }
      if (nBss >= 6)
        {
      // "F" - APs
      positionOutFile << x6 << ", " << y6 << ", " << r6 << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;
        }
      if (nBss >= 7)
        {
      // "G" - APs
      positionOutFile << x7 << ", " << y7 << ", " << r << ", " << csr << std::endl;
      positionOutFile << std::endl;
      positionOutFile << std::endl;
        }

//---------------------------------------各BSSのSTAの配置Position Setting--------------------------------------------------------
      // Network "A"
      // AP1
      positionAlloc->Add (Vector (0.0, 0.0, 0.0)); //Vector(x,y,z) 1つめのAPは原点に配置
      // STAs for AP1
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      //訳：ランダム性の確保のため、unitDiscPositionAllocator1のように各BSSで別のものを用意する
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator1 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator1->AssignStreams (streamNumber);
      // AP1 is at origin (x=0, y=0), with radius Rho=r
      unitDiscPositionAllocator1->SetX (0);
      unitDiscPositionAllocator1->SetY (0);
      unitDiscPositionAllocator1->SetRho (r1); //Multi-BSS scenario (BSS1)
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator1->GetNext (); //default
          //Vector v; //配置の記述(以下3行)
          //v.x = 0;
          //v.y = r1;
          positionAlloc->Add (v); //positionAllocのイメージ→ [0]=APの配置,[1]=属するSTA1の配置,[2]=STA2,[3]=3,[4]=4,.....(STAの配置はfor文でn回分)
          positionOutFile << v.x << ", " << v.y << std::endl;
          std::cout << "BSS1 STA set " <<"(x,y) = " << v.x <<", " << v.y << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;
      if(nBss>=2)
        {
         // Network "B"
         // AP2
         positionAlloc->Add (Vector (x2, y2, 0.0));
         // STAs for AP2
         // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
         // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
         Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator2 = CreateObject<UniformDiscPositionAllocator> ();
         unitDiscPositionAllocator2->AssignStreams (streamNumber + 1);
         // AP1 is at origin (x=x2, y=y2), with radius Rho=r
         unitDiscPositionAllocator2->SetX (x2);
         unitDiscPositionAllocator2->SetY (y2);
         unitDiscPositionAllocator2->SetRho (r2); //Multi-BSS scenario (BSS2)
         for (uint32_t i = 0; i < n; i++)
            {
             Vector v = unitDiscPositionAllocator2->GetNext ();
             //Vector v;
             //v.x = x2;
             //v.y = y2+r2;
             positionAlloc->Add (v);
             positionOutFile << v.x << ", " << v.y << std::endl;
             std::cout << "BSS2 STA set " <<"(x,y) = " << v.x <<", " << v.y << std::endl;
            }
        }
      else
        {
          // need to output something here to represent the positions section for STAs B
          // since the post-processnig script expects there to be something here.
          positionOutFile << d << ", " << 0 << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      if(nBss>=3)
        {
      // Network "C"
      // AP3
      positionAlloc->Add (Vector (x3, y3, 0.0));
      // STAs for AP3
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator3 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator3->AssignStreams (streamNumber + 2);
      // AP1 is at origin (x=x3, y=y3), with radius Rho=r
      unitDiscPositionAllocator3->SetX (x3);
      unitDiscPositionAllocator3->SetY (y3);
      unitDiscPositionAllocator3->SetRho (r3);
      for (uint32_t i = 0; i < n; i++)
        {
         Vector v = unitDiscPositionAllocator3->GetNext ();
         //Vector v;
         //v.x = x3;
         //v.y = y3+r3;
         positionAlloc->Add (v);
         positionOutFile << v.x << ", " << v.y << std::endl;
         std::cout << "BSS3 STA set " <<"(x,y) = " << v.x <<", " << v.y << std::endl;
        }
        }
      else
        {
          // need to output something here to represent the positions section for STAs C
          // since the post-processnig script expects there to be something here.
          positionOutFile << d << ", " << 0 << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;

      if(nBss>=4)
        {
      // Network "D"
      // AP4
      positionAlloc->Add (Vector (x4, y4, 0.0));
      // STAs for AP4
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator4 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator4->AssignStreams (streamNumber + 3);
      // AP1 is at origin (x=x4, y=y4), with radius Rho=r
      unitDiscPositionAllocator4->SetX (x4);
      unitDiscPositionAllocator4->SetY (y4);
      unitDiscPositionAllocator4->SetRho (r4);
      for (uint32_t i = 0; i < n; i++)
        {
         Vector v = unitDiscPositionAllocator4->GetNext ();
         //Vector v;
         //v.x = x4;
         //v.y = y4+r4;
          positionAlloc->Add (v);
          positionOutFile << v.x << ", " << v.y << std::endl;
         std::cout << "BSS4 STA set " <<"(x,y) = " << v.x <<", " << v.y << std::endl;
        }
        }
      else
        {
          // need to output something here to represent the positions section for STAs D
          // since the post-processnig script expects there to be something here.
          positionOutFile << d << ", " << 0 << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;


      if(nBss>=5)
        {
    // Network "E"
      // AP5
      positionAlloc->Add (Vector (x5, y5, 0.0));
      // STAs for AP5
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator5 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator5->AssignStreams (streamNumber + 4);
      // AP1 is at origin (x=x5, y=y5), with radius Rho=r
      unitDiscPositionAllocator5->SetX (x5);
      unitDiscPositionAllocator5->SetY (y5);
      unitDiscPositionAllocator5->SetRho (r5);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator5->GetNext ();
          //Vector v;
          //v.x = x5;
          //v.y = y5+r5;
          positionAlloc->Add (v);
          positionOutFile << v.x << ", " << v.y << std::endl;
          std::cout << "BSS5 STA set " <<"(x,y) = " << v.x <<", " << v.y << std::endl;
        }
        }
      else
        {
          // need to output something here to represent the positions section for STAs E
          // since the post-processnig script expects there to be something here.
          positionOutFile << d << ", " << 0 << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;


      if(nBss>=6)
        {
      // Network "F"
      // AP6
      positionAlloc->Add (Vector (x6, y6, 0.0));
      // STAs for AP6
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator6 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator6->AssignStreams (streamNumber + 5);
      // AP1 is at origin (x=x6, y=y6), with radius Rho=r
      unitDiscPositionAllocator6->SetX (x6);
      unitDiscPositionAllocator6->SetY (y6);
      unitDiscPositionAllocator6->SetRho (r6);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator6->GetNext ();
          //Vector v;
          //v.x = x6;
          //v.y = y6+r6;
          positionAlloc->Add (v);
          positionOutFile << v.x << ", " << v.y << std::endl;
          std::cout << "BSS6 STA set " <<"(x,y) = " << v.x <<", " << v.y << std::endl;
        }
        }
      else
        {
          // need to output something here to represent the positions section for STAs F
          // since the post-processnig script expects there to be something here.
          positionOutFile << d << ", " << 0 << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;


      if(nBss>=7)
        {
      // Network "G"
      // AP7
      positionAlloc->Add (Vector (x7, y7, 0.0));
      // STAs for AP7
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.  To
      // ensure unique randomness of positions,  each allocator must be allocated a different stream number.
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator7 = CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator7->AssignStreams (streamNumber + 6);
      // AP1 is at origin (x=x7, y=y7), with radius Rho=r
      unitDiscPositionAllocator7->SetX (x7);
      unitDiscPositionAllocator7->SetY (y7);
      unitDiscPositionAllocator7->SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator7->GetNext ();
          positionAlloc->Add (v);
          positionOutFile << v.x << ", " << v.y << std::endl;
        }
        }
      else
        {
          // need to output something here to represent the positions section for STAs G
          // since the post-processnig script expects there to be something here.
          positionOutFile << d << ", " << 0 << std::endl;
        }
      positionOutFile << std::endl;
      positionOutFile << std::endl;


      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.SetPositionAllocator (positionAlloc);
      mobility.Install (allNodes);
    }

  positionOutFile << std::endl;
  positionOutFile.close ();
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  double perNodeUplinkMbps = aggregateUplinkMbps / n;
  double perNodeDownlinkMbps = aggregateDownlinkMbps / n;
  Time intervalUplink = MicroSeconds (payloadSizeUplink * 8 / perNodeUplinkMbps);
  Time intervalDownlink = MicroSeconds (payloadSizeDownlink * 8 / perNodeDownlinkMbps);
  std::cout << "Uplink interval:" << intervalUplink << " Downlink interval:" << intervalDownlink << std::endl;
  std::cout << "ApplicationTxStart: " << applicationTxStart << " Duration: " << duration << std::endl;
  std::cout << "nBss: " << nBss << " nStas/Bss: " << n << " => nStas: " << n * nBss << std::endl;

  Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable> ();
  // assign stream to prevent perturbations
  urv->SetAttribute ("Stream", IntegerValue (200));
  urv->SetAttribute ("Min", DoubleValue (-txStartOffset));
  urv->SetAttribute ("Max", DoubleValue (txStartOffset));

  /* Internet stack */
  uint64_t stackStream = 900;
  InternetStackHelper stack;
  stack.Install (nodesA);
  stack.AssignStreams (nodesA, stackStream + 0);
  stack.Install (nodesB);
  stack.AssignStreams (nodesB, stackStream + 1);
  stack.Install (nodesC);
  stack.AssignStreams (nodesC, stackStream + 2);
  stack.Install (nodesD);
  stack.AssignStreams (nodesD, stackStream + 3);
  stack.Install (nodesE);
  stack.AssignStreams (nodesE, stackStream + 4);
  stack.Install (nodesF);
  stack.AssignStreams (nodesF, stackStream + 5);
  stack.Install (nodesG);
  stack.AssignStreams (nodesG, stackStream + 6);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer StaInterfaceA;
  StaInterfaceA = address.Assign (staDevicesA);
  Ipv4InterfaceContainer ApInterfaceA;
  ApInterfaceA = address.Assign (apDeviceA);
  Ipv4InterfaceContainer StaInterfaceB;
  StaInterfaceB = address.Assign (staDevicesB);
  Ipv4InterfaceContainer ApInterfaceB;
  ApInterfaceB = address.Assign (apDeviceB);
  Ipv4InterfaceContainer StaInterfaceC;
  StaInterfaceC = address.Assign (staDevicesC);
  Ipv4InterfaceContainer ApInterfaceC;
  ApInterfaceC = address.Assign (apDeviceC);
  Ipv4InterfaceContainer StaInterfaceD;
  StaInterfaceD = address.Assign (staDevicesD);
  Ipv4InterfaceContainer ApInterfaceD;
  ApInterfaceD = address.Assign (apDeviceD);
  // an additional address help to prevent address overflow
  Ipv4AddressHelper address2;
  address2.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer StaInterfaceE;
  StaInterfaceE = address2.Assign (staDevicesE);
  Ipv4InterfaceContainer ApInterfaceE;
  ApInterfaceE = address2.Assign (apDeviceE);
  Ipv4InterfaceContainer StaInterfaceF;
  StaInterfaceF = address2.Assign (staDevicesF);
  Ipv4InterfaceContainer ApInterfaceF;
  ApInterfaceF = address2.Assign (apDeviceF);
  Ipv4InterfaceContainer StaInterfaceG;
  StaInterfaceG = address2.Assign (staDevicesG);
  Ipv4InterfaceContainer ApInterfaceG;
  ApInterfaceG = address2.Assign (apDeviceG);

  /* Setting applications */
  uint16_t uplinkPortA = 9;
  uint16_t downlinkPortA = 10;
  UdpServerHelper uplinkServerA (uplinkPortA);
  UdpServerHelper downlinkServerA (downlinkPortA);
  uint16_t uplinkPortB = 11;
  uint16_t downlinkPortB = 12;
  UdpServerHelper uplinkServerB (uplinkPortB);
  UdpServerHelper downlinkServerB (downlinkPortB);
  uint16_t uplinkPortC = 13;
  uint16_t downlinkPortC = 14;
  UdpServerHelper uplinkServerC (uplinkPortC);
  UdpServerHelper downlinkServerC (downlinkPortC);
  uint16_t uplinkPortD = 15;
  uint16_t downlinkPortD = 16;
  UdpServerHelper uplinkServerD (uplinkPortD);
  UdpServerHelper downlinkServerD (downlinkPortD);
  uint16_t uplinkPortE = 17;
  uint16_t downlinkPortE = 18;
  UdpServerHelper uplinkServerE (uplinkPortE);
  UdpServerHelper downlinkServerE (downlinkPortE);
  uint16_t uplinkPortF = 19;
  uint16_t downlinkPortF = 20;
  UdpServerHelper uplinkServerF (uplinkPortF);
  UdpServerHelper downlinkServerF (downlinkPortF);
  uint16_t uplinkPortG = 21;
  uint16_t downlinkPortG = 22;
  UdpServerHelper uplinkServerG (uplinkPortG);
  UdpServerHelper downlinkServerG (downlinkPortG);

  if ((payloadSizeUplink > 0) || (payloadSizeDownlink > 0))
    {
      //BSS 1

      for (uint32_t i = 0; i < n; i++)
        {
          if (aggregateUplinkMbps > 0)
            {
              AddClient (uplinkClientApps, ApInterfaceA.GetAddress (0), stasA.Get (i), uplinkPortA, intervalUplink, payloadSizeUplink, urv, txStartOffset);
            }
          if (aggregateDownlinkMbps > 0)
            {
              AddClient (downlinkClientApps, StaInterfaceA.GetAddress (i), ap1, downlinkPortA, intervalDownlink, payloadSizeDownlink, urv, txStartOffset);
              AddServer (downlinkServerApps, downlinkServerA, stasA.Get (i));
            }
        }
      if (aggregateUplinkMbps > 0)
      {
        AddServer (uplinkServerApps, uplinkServerA, ap1);
      }
    }

  if (((payloadSizeUplink > 0) || (payloadSizeDownlink > 0)) && (nBss >= 2))
    {
      // BSS 2

      for (uint32_t i = 0; i < n; i++)
        {
          if (aggregateUplinkMbps > 0)
            {
              AddClient (uplinkClientApps, ApInterfaceB.GetAddress (0), stasB.Get (i), uplinkPortB, intervalUplink, payloadSizeUplink, urv, txStartOffset);
            }
          if (aggregateDownlinkMbps > 0)
            {
              AddClient (downlinkClientApps, StaInterfaceB.GetAddress (i), ap2, downlinkPortB, intervalDownlink, payloadSizeDownlink, urv, txStartOffset);
              AddServer (downlinkServerApps, downlinkServerB, stasB.Get (i));

            }
        }
      if (aggregateUplinkMbps > 0)
      {
        AddServer (uplinkServerApps, uplinkServerB, ap2);
      }
    }

  if (((payloadSizeUplink > 0) || (payloadSizeDownlink > 0)) && (nBss >= 3))
    {
      // BSS 3

      for (uint32_t i = 0; i < n; i++)
        {
          if (aggregateUplinkMbps > 0)
            {
              AddClient (uplinkClientApps, ApInterfaceC.GetAddress (0), stasC.Get (i), uplinkPortC, intervalUplink, payloadSizeUplink, urv, txStartOffset);
            }
          if (aggregateDownlinkMbps > 0)
            {
              AddClient (downlinkClientApps, StaInterfaceC.GetAddress (i), ap3, downlinkPortC, intervalDownlink, payloadSizeDownlink, urv, txStartOffset);
              AddServer (downlinkServerApps, downlinkServerC, stasC.Get (i));
            }
        }
      if (aggregateUplinkMbps > 0)
      {
        AddServer (uplinkServerApps, uplinkServerC, ap3);
      }
    }

  if (((payloadSizeUplink > 0) || (payloadSizeDownlink > 0)) && (nBss >= 4))
    {
      // BSS 4

      for (uint32_t i = 0; i < n; i++)
        {
          if (aggregateUplinkMbps > 0)
            {
              AddClient (uplinkClientApps, ApInterfaceD.GetAddress (0), stasD.Get (i), uplinkPortD, intervalUplink, payloadSizeUplink, urv, txStartOffset);
            }
          if (aggregateDownlinkMbps > 0)
            {
              AddClient (downlinkClientApps, StaInterfaceD.GetAddress (i), ap4, downlinkPortD, intervalDownlink, payloadSizeDownlink, urv, txStartOffset);
              AddServer (downlinkServerApps, downlinkServerD, stasD.Get (i));
            }
        }
      if (aggregateUplinkMbps > 0)
      {
        AddServer (uplinkServerApps, uplinkServerD, ap4);
      }
    }

  if (((payloadSizeUplink > 0) || (payloadSizeDownlink > 0)) && (nBss >= 5))
    {
      // BSS 5

      for (uint32_t i = 0; i < n; i++)
        {
          if (aggregateUplinkMbps > 0)
            {
              AddClient (uplinkClientApps, ApInterfaceE.GetAddress (0), stasE.Get (i), uplinkPortE, intervalUplink, payloadSizeUplink, urv, txStartOffset);
            }
          if (aggregateDownlinkMbps > 0)
            {
              AddClient (downlinkClientApps, StaInterfaceE.GetAddress (i), ap5, downlinkPortE, intervalDownlink, payloadSizeDownlink, urv, txStartOffset);
              AddServer (downlinkServerApps, downlinkServerE, stasE.Get (i));
            }
        }
      if (aggregateUplinkMbps > 0)
      {
        AddServer (uplinkServerApps, uplinkServerE, ap5);
      }
    }
  
  if (((payloadSizeUplink > 0) || (payloadSizeDownlink > 0)) && (nBss >= 6))
    {
      // BSS 6

      for (uint32_t i = 0; i < n; i++)
        {
          if (aggregateUplinkMbps > 0)
            {
              AddClient (uplinkClientApps, ApInterfaceF.GetAddress (0), stasF.Get (i), uplinkPortF, intervalUplink, payloadSizeUplink, urv, txStartOffset);
            }
          if (aggregateDownlinkMbps > 0)
            {
              AddClient (downlinkClientApps, StaInterfaceF.GetAddress (i), ap6, downlinkPortF, intervalDownlink, payloadSizeDownlink, urv, txStartOffset);
              AddServer (downlinkServerApps, downlinkServerF, stasF.Get (i));
            }
        }
      if (aggregateUplinkMbps > 0)
      {
        AddServer (uplinkServerApps, uplinkServerF, ap6);
      }
    }
  
  if (((payloadSizeUplink > 0) || (payloadSizeDownlink > 0)) && (nBss >= 7))
    {
      // BSS 7

      for (uint32_t i = 0; i < n; i++)
        {
          if (aggregateUplinkMbps > 0)
            {
              AddClient (uplinkClientApps, ApInterfaceG.GetAddress (0), stasG.Get (i), uplinkPortG, intervalUplink, payloadSizeUplink, urv, txStartOffset);
            }
          if (aggregateDownlinkMbps > 0)
            {
              AddClient (downlinkClientApps, StaInterfaceG.GetAddress (i), ap7, downlinkPortG, intervalDownlink, payloadSizeDownlink, urv, txStartOffset);
              AddServer (downlinkServerApps, downlinkServerG, stasG.Get (i));
            }
        }
      if (aggregateUplinkMbps > 0)
      {
        AddServer (uplinkServerApps, uplinkServerG, ap7);
      }
    }

  Ptr<FlowMonitor> monitorA = flowmonHelperA.Install (nodesA);
  if (monitorA != 0)
    {
      monitorA->SetAttribute ("DelayBinWidth", DoubleValue (0.001));
      monitorA->SetAttribute ("JitterBinWidth", DoubleValue (0.001));
      monitorA->SetAttribute ("PacketSizeBinWidth", DoubleValue (20));
    }

  uplinkServerApps.Start (Seconds (0.0));
  uplinkClientApps.Start (Seconds (applicationTxStart));
  downlinkServerApps.Start (Seconds (0.0));
  downlinkClientApps.Start (Seconds (applicationTxStart));

  for (uint32_t i = 0; i < ((n + 1) * nBss); i++)
    {
      if (i < (n + 1)) // BSS 1
        {
          std::stringstream stmp;
          stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
          Config::Set (stmp.str(), UintegerValue (std::min(maxAmpduSizeBss1, 4194303u)));
        }
      else if (i < (2 * (n + 1))) // BSS 2
        {
          std::stringstream stmp;
          stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
          Config::Set (stmp.str(), UintegerValue (std::min(maxAmpduSizeBss2, 4194303u)));
        }
      else if (i < (3 * (n + 1))) // BSS 3
        {
          std::stringstream stmp;
          stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
          Config::Set (stmp.str(), UintegerValue (std::min(maxAmpduSizeBss3, 4194303u)));
        }
      else if (i < (4 * (n + 1))) // BSS 4
        {
          std::stringstream stmp;
          stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
          Config::Set (stmp.str(), UintegerValue (std::min(maxAmpduSizeBss4, 4194303u)));
        }
      else if (i < (5 * (n + 1))) // BSS 5
        {
          std::stringstream stmp;
          stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
          Config::Set (stmp.str(), UintegerValue (std::min(maxAmpduSizeBss5, 4194303u)));
        }
      else if (i < (6 * (n + 1))) // BSS 6
        {
          std::stringstream stmp;
          stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
          Config::Set (stmp.str(), UintegerValue (std::min(maxAmpduSizeBss6, 4194303u)));
        }
      else if (i < (7 * (n + 1))) // BSS 7
        {
          std::stringstream stmp;
          stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
          Config::Set (stmp.str(), UintegerValue (std::min(maxAmpduSizeBss7, 4194303u)));
        }
    }
  
  //Config::Connect ("/NodeList/*/DeviceList/*/Phy/MonitorSnifferRx", MakeCallback (&MonitorSniffRx));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback (&PacketTx));
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpServer/RxWithAddresses", MakeCallback (&PacketRx));
  //Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/ObssPdAlgorithm/Reset", MakeCallback (&PhyReset));

  if (performTgaxTimingChecks)
    {
      Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/MacLow/TxAmpdu", MakeCallback (&TxAmpduCallback));
      Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/MacLow/RxBlockAck", MakeCallback (&RxBlockAckCallback));
    }

  if (enableAscii)
    {
      AsciiTraceHelper ascii;
      spectrumPhy.EnableAsciiAll (ascii.CreateFileStream (outputFilePrefix + "-" + testname + ".tr"));
    }

  // This enabling function could be scheduled later in simulation if desired
  SchedulePhyLogConnect ();
  g_stateFile.open (outputFilePrefix + "-state-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
  g_stateFile.setf (std::ios_base::fixed);
  ScheduleStateLogConnect ();
  ScheduleAddbaStateLogConnect ();
  ScheduleStaAssocLogConnect();


      // Trace backoff evolution
Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/BackoffTrace", MakeCallback (&BackoffTrace));
Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/CwTrace", MakeCallback (&CwTrace));
  g_rxSniffFile.open (outputFilePrefix + "-rx-sniff-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
  g_rxSniffFile.setf (std::ios_base::fixed);
  g_rxSniffFile << "RxNodeId, DstNodeId, SrcNodeId, RxNodeAddr, DA, SA, Noise, Signal " << std::endl;

  g_txPowerFile.open (outputFilePrefix + "-tx-power-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
  g_txPowerFile.setf (std::ios_base::fixed);
  g_txPowerFile << "RxNodeId, DstNodeId, TxPowerDbm " << std::endl;

  if (enableObssPd)
    {
      g_phyResetFile.open (outputFilePrefix + "-phy-reset-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
      g_phyResetFile.setf (std::ios_base::fixed);
      g_phyResetFile << "Time, NodeId, BssColor, RssiDbm, TxPowerMaxDbmSiso, txPowerMaxDbmMimo" << std::endl;
    }

  g_TGaxCalibrationTimingsFile.open (outputFilePrefix + "-tgax-calibration-timings-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
  g_TGaxCalibrationTimingsFile.setf (std::ios_base::fixed);

  // Save attribute configuration
  Config::SetDefault ("ns3::ConfigStore::Filename", StringValue (outputFilePrefix + ".config"));
  Config::SetDefault ("ns3::ConfigStore::FileFormat", StringValue ("RawText"));
  Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Save"));
  ConfigStore outputConfig;
  outputConfig.ConfigureAttributes ();

  if (enablePcap)
    {
      spectrumPhy.EnablePcap ("STA_pcap", staDevicesA);
      spectrumPhy.EnablePcap ("AP_pcap", apDeviceA);
    }
      // Trace CW evolution
    

  if (disableArp)
    {
      PopulateArpCache ();
    }

  Time durationTime = Seconds (duration + applicationTxStart);
  Simulator::Stop (durationTime + Seconds (100)); //protection to make sure simulation finishes oneday
  Simulator::Run ();

  SchedulePhyLogDisconnect ();
  ScheduleStateLogDisconnect ();
  ScheduleAddbaStateLogDisconnect ();
  ScheduleStaAssocLogDisconnect();
Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/BackoffTrace", MakeCallback (&BackoffTrace));
Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/CwTrace", MakeCallback (&CwTrace));
  g_stateFile.flush ();
  g_stateFile.close ();

  g_TGaxCalibrationTimingsFile.close ();

  SaveSpectrumPhyStats (outputFilePrefix + "-phy-log-" + testname + ".dat", g_arrivals);

  if (!allStasAssociated)
  {
    NS_FATAL_ERROR ("Not all STAs are associated at the end of the simulation");
  }
  if (filterOutNonAddbaEstablished && !allAddBaEstablished)
  {
    NS_FATAL_ERROR ("filterOutNonAddbaEstablished option enabled but not all ADDBA hanshakes are established at the end of the simulation");
  }

  // Save spatial reuse statistics to an output file
  SaveSpatialReuseStats (outputFilePrefix + "-SR-stats-" + testname + ".dat", d,  r1, r2, r3, r4, r5, r6, freq, csr, scenario, aggregateUplinkMbps, aggregateDownlinkMbps); //Multi-BSS scenario(r1とr2を保存するように変更)

  // save flow-monitor results
  std::stringstream stmp;
  stmp << outputFilePrefix + "-A-" + testname + ".flowmon";

  if (monitorA != 0)
    {
      monitorA->SerializeToXmlFile (stmp.str ().c_str (), true, true);
    }

  SaveUdpFlowMonitorStats (outputFilePrefix + "-operatorA-" + testname, "simulationParams", monitorA, flowmonHelperA, durationTime.GetSeconds ());

  Simulator::Destroy ();
  return 0;
}


