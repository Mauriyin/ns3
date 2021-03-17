/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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
#include <ns3/flow-monitor-module.h>
#include <ns3/flow-monitor-helper.h>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("SpatialReuse");

#define GET_GLOB_VAL(type, glob, val) {type _;glob.GetValue(_);val=_.Get();}
#define GET_GLOB_VAL_G(type, name) GET_GLOB_VAL(type, g_##name, name)

const uint32_t MAX_NBSS = 7;

struct SignalArrival
{
  Time m_time;
  Time m_duration;
  uint32_t m_nodeId;
  uint32_t m_senderNodeId;
  double m_power;
  bool m_wifi;
};

// Global variables for use in callbacks.
bool allAddBaEstablished = false;
bool g_logArrivals = false;

double g_arrivalsDurationCounter = 0;

Time timeAllAddBaEstablished;
Time timeLastAmpduDuration = Seconds (0);
Time timeLastBlockAckDuration = Seconds (-1);
Time timeLastDeferAndBackoffDuration = Seconds (-1);
Time timeLastPacketReceived;
Time timeLastRxBlockAckEnd = Seconds (0);
Time timeLastSifsDuration = Seconds (-1);
Time timeLastTxAmpduEnd = Seconds (0);

std::ofstream g_phyResetFile;
std::ofstream g_rxSniffFile;
std::ofstream g_stateFile;
std::ofstream g_TGaxCalibrationTimingsFile;
std::ofstream g_txPowerFile;

std::vector<uint32_t> signals (100);
std::vector<uint32_t> noises (100);

std::vector<SignalArrival> g_arrivals;

// Global parameters
static GlobalValue
g_aggregateDownlinkMbps ("downlink",
  "Aggregate downlink load, AP-STAs (Mbps)",
  DoubleValue (1.0),
  MakeDoubleChecker<double> (0.0)
);

static GlobalValue
g_aggregateUplinkMbps ("uplink",
  "Aggregate uplink load, STAs-AP (Mbps)",
  DoubleValue (1.0),
  MakeDoubleChecker<double> (0.0)
);

static GlobalValue
g_antennas ("antennas",
  "The number of antennas on each device",
  UintegerValue (1),
  MakeUintegerChecker<uint16_t> (0)
);

static GlobalValue
g_applicationTxStart ("applicationTxStart",
  "Time (s) to allow network to reach steady-state before applications start sending packets",
  DoubleValue (1.0),
  MakeDoubleChecker<double> (0.0)
);

static GlobalValue
g_beaconInterval ("beaconInterval",
  "Beacon interval in microseconds",
  UintegerValue (102400),
  MakeUintegerChecker<uint32_t> (0)
);

static GlobalValue
g_bianchi ("bianchi",
  "Set parameters for Biachi validation", 
  BooleanValue (false),
  MakeBooleanChecker ()
);

static GlobalValue
g_bw ("bw",
  "Bandwidth (consistent with standard, in MHz)",
  IntegerValue (20),
  MakeIntegerChecker<int32_t> (0)
);

static GlobalValue
g_ccaTrAp ("ccaTrAp",
  "CCA Threshold of AP (dBm)",
  DoubleValue (-62.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_ccaTrSta ("ccaTrSta",
  "CCA Threshold of STA (dBm)",
  DoubleValue (-62.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue g_colorBss1 ("colorBss1", "The color for BSS 1", UintegerValue (1), MakeUintegerChecker<uint16_t> (0));
static GlobalValue g_colorBss2 ("colorBss2", "The color for BSS 2", UintegerValue (2), MakeUintegerChecker<uint16_t> (0));
static GlobalValue g_colorBss3 ("colorBss3", "The color for BSS 3", UintegerValue (3), MakeUintegerChecker<uint16_t> (0));
static GlobalValue g_colorBss4 ("colorBss4", "The color for BSS 4", UintegerValue (4), MakeUintegerChecker<uint16_t> (0));
static GlobalValue g_colorBss5 ("colorBss5", "The color for BSS 5", UintegerValue (5), MakeUintegerChecker<uint16_t> (0));
static GlobalValue g_colorBss6 ("colorBss6", "The color for BSS 6", UintegerValue (6), MakeUintegerChecker<uint16_t> (0));
static GlobalValue g_colorBss7 ("colorBss7", "The color for BSS 7", UintegerValue (7), MakeUintegerChecker<uint16_t> (0));

static GlobalValue
g_d ("d",
  "Distance between adjacency APs (m)",
  DoubleValue (100.0),
  MakeDoubleChecker<double> (0.0)
);

static GlobalValue
g_disableArp ("disableArp",
  "Flag whether we disable ARP mechanism (populate cache before simulation starts and set a very high timeout)", 
  BooleanValue (true),
  MakeBooleanChecker ()
);

static GlobalValue
g_duration ("duration",
  "Data of simulation (s)",
  DoubleValue (5.0),
  MakeDoubleChecker<double> (0.0)
);

static GlobalValue
g_enableAscii ("enableAscii",
  "Enable ASCII trace file generation", 
  BooleanValue (false),
  MakeBooleanChecker ()
);

static GlobalValue
g_enableFrameCapture ("enableFrameCapture",
  "Enable or disable frame capture", 
  BooleanValue (true),
  MakeBooleanChecker ()
);

static GlobalValue
g_enableObssPd ("enableObssPd",
  "Enable or disable OBSS_PD", 
  BooleanValue (false),
  MakeBooleanChecker ()
);

static GlobalValue
g_enablePcap ("enablePcap",
  "Enable PCAP trace file generation", 
  BooleanValue (false),
  MakeBooleanChecker ()
);

static GlobalValue
g_enableRts ("enableRts",
  "Enable or disable RTS/CTS", 
  BooleanValue (false),
  MakeBooleanChecker ()
);

static GlobalValue
g_enableThresholdPreambleDetection ("enableThresholdPreambleDetection",
  "Enable or disable threshold-based preamble detection (if not set, preamble detection is always successful)", 
  BooleanValue (true),
  MakeBooleanChecker ()
);

static GlobalValue
g_filterOutNonAddbaEstablished ("filterOutNonAddbaEstablished",
  "Flag whether statistics obtained before all ADDBA hanshakes have been established are filtered out", 
  BooleanValue (false),
  MakeBooleanChecker ()
);

static GlobalValue
g_fixedPosition ("fixedPosition",
  "Fixed STA location relative to AP", 
  BooleanValue (false),
  MakeBooleanChecker ()
);

static GlobalValue g_maxAmpduSizeBss1 ("maxAmpduSizeBss1", "The maximum A-MPDU size for BSS 1 (bytes)", UintegerValue (65535u), MakeUintegerChecker<uint32_t> (0));
static GlobalValue g_maxAmpduSizeBss2 ("maxAmpduSizeBss2", "The maximum A-MPDU size for BSS 2 (bytes)", UintegerValue (65535u), MakeUintegerChecker<uint32_t> (0));
static GlobalValue g_maxAmpduSizeBss3 ("maxAmpduSizeBss3", "The maximum A-MPDU size for BSS 3 (bytes)", UintegerValue (65535u), MakeUintegerChecker<uint32_t> (0));
static GlobalValue g_maxAmpduSizeBss4 ("maxAmpduSizeBss4", "The maximum A-MPDU size for BSS 4 (bytes)", UintegerValue (65535u), MakeUintegerChecker<uint32_t> (0));
static GlobalValue g_maxAmpduSizeBss5 ("maxAmpduSizeBss5", "The maximum A-MPDU size for BSS 5 (bytes)", UintegerValue (65535u), MakeUintegerChecker<uint32_t> (0));
static GlobalValue g_maxAmpduSizeBss6 ("maxAmpduSizeBss6", "The maximum A-MPDU size for BSS 6 (bytes)", UintegerValue (65535u), MakeUintegerChecker<uint32_t> (0));
static GlobalValue g_maxAmpduSizeBss7 ("maxAmpduSizeBss7", "The maximum A-MPDU size for BSS 7 (bytes)", UintegerValue (65535u), MakeUintegerChecker<uint32_t> (0));

static GlobalValue
g_maxExpectedThroughputBss1Down ("maxExpectedThroughputBss1Down",
  "The maximum expected downstream throughput in Mbps for BSS 1 (used by regression)",
  DoubleValue (0.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_maxExpectedThroughputBss1Up ("maxExpectedThroughputBss1Up",
  "The maximum expected upstream throughput in Mbps for BSS 1 (used by regression)",
  DoubleValue (0.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_maxMissedBeacons ("maxMissedBeacons",
  "Number of beacons which much be consecutively missed before association is restarted",
  UintegerValue (10),
  MakeUintegerChecker<uint32_t> ()
);

static GlobalValue
g_maxQueueDelay ("maxQueueDelay",
  "If a packet stays longer than this delay in the queue, it is dropped",
  UintegerValue (500),
  MakeUintegerChecker<uint64_t> (0)
);

static GlobalValue
g_maxSlrc ("maxSlrc",
  "MaxSlrc",
  IntegerValue (7),
  MakeIntegerChecker<int32_t> ()
);

static GlobalValue
g_maxSupportedRxSpatialStreams ("maxSupportedRxSpatialStreams",
  "The maximum number of supported Rx spatial streams",
  UintegerValue (1),
  MakeUintegerChecker<uint16_t> (1)
);

static GlobalValue
g_maxSupportedTxSpatialStreams ("maxSupportedTxSpatialStreams",
  "The maximum number of supported Tx spatial streams",
  UintegerValue (1),
  MakeUintegerChecker<uint16_t> (1)
);

static GlobalValue
g_mcs ("MCS",
  "Modulation and Coding Scheme (MCS) index (default = 0)",
  UintegerValue (0),
  MakeUintegerChecker<uint16_t> (0)
);

static GlobalValue
g_minExpectedThroughputBss1Down ("minExpectedThroughputBss1Down",
  "The minimum expected downstream throughput in Mbps for BSS 1 (used by regression)",
  DoubleValue (0.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_minExpectedThroughputBss1Up ("minExpectedThroughputBss1Up",
  "The minimum expected upstream throughput in Mbps for BSS 1 (used by regression)",
  DoubleValue (0.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_n ("n",
  "Number of STAs to scatter around each AP",
  UintegerValue (1),
  MakeUintegerChecker<uint32_t> (1)
);

static GlobalValue
g_nBss ("nBss",
  "The number of BSSs",
  UintegerValue (1),
  MakeUintegerChecker<uint32_t> (1, MAX_NBSS)
);

static GlobalValue
g_obssPdAlgorithm ("obssPdAlgorithm",
  "OBSS PD Algorithm (ConstantObssPdAlgorithm, DynamicObssPdAlgorithm)",
  StringValue ("DynamicObssPdAlgorithm"),
  MakeStringChecker ()
);

static GlobalValue
g_payloadSizeDownlink ("payloadSizeDownlink",
  "Payload size of 1 downlink packet (bytes)",
  UintegerValue (300),
  MakeUintegerChecker<uint32_t> (0)
);

static GlobalValue
g_payloadSizeUplink ("payloadSizeUplink",
  "Payload size of 1 uplink packet (bytes)",
  UintegerValue (1500),
  MakeUintegerChecker<uint32_t> (0)
);

static GlobalValue
g_performTgaxTimingChecks ("performTgaxTimingChecks",
  "Perform TGax timings checks (for MAC simulation calibrations)",
  BooleanValue (false),
  MakeBooleanChecker ()
);

static GlobalValue
g_powAp ("powAp",
  "Power of AP (dBm)",
  DoubleValue (20.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_powerBackoff ("powerBackoff",
  "Enable/disable OBSS_PD SR power backoff",
  BooleanValue (true),
  MakeBooleanChecker ()
);

static GlobalValue
g_powSta ("powSta",
  "Power of STA (dBm)",
  DoubleValue (20.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_r ("r",
  "Radius of circle around each AP in which to scatter STAs (m)",
  DoubleValue (50.0),
  MakeDoubleChecker<double> (0.0)
);

static GlobalValue
g_rxGain ("rxGain",
  "Reception gain (dBi)",
  DoubleValue (0.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_rxSensitivity ("rxSensitivity",
  "Reception sensitivity (dBm)",
  DoubleValue (-90.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_standard ("standard",
  "Set standard (802.11a, 802.11b, 802.11g, 802.11n-5GHz, 802.11n-2.4GHz, 802.11ac, 802.11-holland, 802.11-10MHz, 802.11-5MHz, 802.11ax-5GHz, 802.11ax-2.4GHz)",
  StringValue ("11ac"),
  MakeStringChecker ()
);

static GlobalValue
g_txGain ("txGain",
  "Transmission gain (dBi)",
  DoubleValue (0.0),
  MakeDoubleChecker<double> ()
);

static GlobalValue
g_txRange ("txRange",
  "Max TX range (m)",
  DoubleValue (54.0),
  MakeDoubleChecker<double> (0.0)
);

static GlobalValue
g_txStartOffset ("txStartOffset",
  "N(0, mu) offset for each node's start of packet transmission, default mu=5 (ns)",
  DoubleValue (5.0),
  MakeDoubleChecker<double> (0.0)
);

static GlobalValue
g_useExplicitBarAfterMissedBlockAck ("useExplicitBarAfterMissedBlockAck",
  "Flag whether statistics obtained before all ADDBA hanshakes have been established are filtered out", 
  BooleanValue (true),
  MakeBooleanChecker ()
);

static GlobalValue
g_useIdealWifiManager ("useIdealWifiManager",
  "Use IdealWifiManager instead of ConstantRateWifiManager", 
  BooleanValue (false),
  MakeBooleanChecker ()
);

static GlobalValue
g_verifyResults ("verifyResults",
  "Flag used by regression to indicate resuts should be verified at the end of the simulation run", 
  BooleanValue (false),
  MakeBooleanChecker ()
);

// associated stations
bool allStasAssociated = false;
std::vector<uint32_t> nAssociatedStasPerBss (0);

std::vector<Time> busyTime;

// for tracking packets and bytes received. will be reallocated once we finalize number of nodes
std::vector<uint64_t> packetsReceived;
std::vector<uint64_t> bytesReceived;
std::vector<uint64_t> bytesTransmitted;

std::vector< std::vector<uint64_t> > packetsReceivedPerNode;
std::vector< std::vector<double> > rssiPerNode;

NetDeviceContainer apDevices[MAX_NBSS];
NetDeviceContainer staDevices[MAX_NBSS];
NodeContainer allNodes;
ApplicationContainer uplinkServerApps;
ApplicationContainer downlinkServerApps;
ApplicationContainer uplinkClientApps;
ApplicationContainer downlinkClientApps;

bool filterOutNonAddbaEstablished;

double aggregateUplinkMbps;
double aggregateDownlinkMbps;
double duration;

uint32_t beaconInterval;
uint32_t nBss;
uint32_t n;

// Find nodeId given a MacAddress
int
MacAddressToNodeId (Mac48Address macAddress)
{
  // int nodeId = -1;
  // uint32_t nNodes = allNodes.GetN ();
  // for (uint32_t n = 0; n < nNodes; n++)
  //   {
  //     Mac48Address nodeAddress = Mac48Address::ConvertFrom (allNodes.Get (n)->GetDevice (0)->GetAddress ());
  //     if (macAddress == nodeAddress)
  //       {
  //         nodeId = n;
  //         break;
  //       }
  //   }
  static std::map<Mac48Address, int32_t> mac2node;
  static bool isFirstCall = true;
  if(isFirstCall)
    {
      uint32_t nNodes = allNodes.GetN ();
      for (uint32_t n = 0; n < nNodes; n++)
      {
        Mac48Address nodeAddress = Mac48Address::ConvertFrom (allNodes.Get (n)->GetDevice (0)->GetAddress ());
        mac2node[nodeAddress] = n;
        // std::cout << "Node " << n << ", MAC " << nodeAddress << std::endl;
      }
      isFirstCall = false;
    }

  return mac2node[macAddress];
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

// Parse context strings of the form "/NodeList/3/DeviceList/1/Mac/Assoc" to extract the NodeId
uint32_t
ContextToNodeId (std::string context)
{
  std::string sub = context.substr (10); // skip "/NodeList/"
  uint32_t pos = sub.find ("/Device");
  uint32_t nodeId = atoi (sub.substr (0, pos).c_str ());
  NS_LOG_DEBUG ("Found NodeId " << nodeId);
  return nodeId;
}

uint32_t
Ipv4AddressToNodeId (Ipv4Address address)
{
  std::stringstream address_str;
  address.Print (address_str);
  std::string sub = address_str.str().substr (10);
  uint32_t nodeId = atoi (sub.c_str ());
  if ( nodeId % (n + 1) == 0)
    {
      nodeId = (nodeId / (n + 1) - 1) * (n + 1);
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
WriteCalibrationResult (std::string context, std::string type, Time duration,  Ptr<const Packet> p, const WifiMacHeader &hdr)
{
  Time t_now = Simulator::Now ();
  g_TGaxCalibrationTimingsFile << "---------" << std::endl;
  g_TGaxCalibrationTimingsFile << *p << " " << hdr << std::endl;
  g_TGaxCalibrationTimingsFile << t_now << " " << context << " " << type << " " << duration << std::endl;
}

void
TxAmpduCallback (std::string context, Ptr<const Packet> p, const WifiMacHeader &hdr)
{
  Time t_now = Simulator::Now ();
  Time t_duration = hdr.GetDuration ();

  // TGax calibration checkpoint calculations
  Time t_cp1 = t_now;
  Time t_cp2 = t_now + t_duration;
  Time ampduDuration = t_cp2 - t_cp1; // same as t_duration

  WriteCalibrationResult (context, "A-MPDU-duration", ampduDuration, p, hdr);
  timeLastAmpduDuration = ampduDuration;
  timeLastTxAmpduEnd = t_cp2;

  if (timeLastRxBlockAckEnd > Seconds (0))
    {
      Time t_cp4 = t_now;
      Time t_cp3 = timeLastRxBlockAckEnd;

      Time deferAndBackoffDuration = t_cp4 - t_cp3;
      WriteCalibrationResult (context, "Defer-and-backoff-duration", deferAndBackoffDuration, p, hdr);
      timeLastDeferAndBackoffDuration = deferAndBackoffDuration;
    }
}

void
RxBlockAckCallback (std::string context, Ptr<const Packet> p, const WifiMacHeader &hdr)
{
  Time t_now = Simulator::Now ();
  Time t_duration = hdr.GetDuration ();

  // TGax calibration checkpoint calculations
  Time t_cp3 = t_now;
  Time t_cp4 = t_now + t_duration;

  Time sifsDuration = t_cp3 - timeLastTxAmpduEnd;
  WriteCalibrationResult (context, "Sifs-duration", sifsDuration, p, hdr);
  timeLastSifsDuration = sifsDuration;

  WriteCalibrationResult (context, "Block-ACK-duration", t_duration, p, hdr);
  timeLastBlockAckDuration = t_duration;
  timeLastRxBlockAckEnd = t_cp4;
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
  g_stateFile << ContextToNodeId (context) << " " << start.GetSeconds () << " " << duration.GetSeconds () << " " << state << std::endl;
}

void
AddbaStateCb (std::string context, Time t, Mac48Address recipient, uint8_t tid, OriginatorBlockAckAgreement::State state)
{
  static uint32_t nEstablishedAddaBa = 0;
  bool isAp = (ContextToNodeId (context)) % (n + 1) == 0;

  if (state == OriginatorBlockAckAgreement::ESTABLISHED)
    {
      if ((aggregateDownlinkMbps != 0) && (aggregateUplinkMbps != 0)) // UP + DL
        {
          nEstablishedAddaBa ++;
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
      else if (((aggregateDownlinkMbps != 0) && isAp) || ((aggregateUplinkMbps != 0) && !isAp)) // UL or DL
        {
          nEstablishedAddaBa ++;
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
      // Make sure ADDBA establishment will be restarted
      Ptr<UdpClient> client;
      if (isAp && (aggregateDownlinkMbps != 0))
        {
          // TODO: only do this for the recipient
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
}

void
StaAssocCb (std::string context, Mac48Address bssid)
{
  static uint32_t nAssociatedStas = 0;

  // Node ID
  uint32_t nodeId = ContextToNodeId (context);
  // BSS ID
  uint32_t bss = nodeId / (n + 1);
  // Determine application ID from node ID
  uint32_t appId = nodeId - bss - 1;

  // std::cout << "[" << nodeId << "] bss: " << bss << ", app: " << appId << ", nAssociatedStas " << (nAssociatedStas + 1) << std::endl;

  nAssociatedStasPerBss[bss]++;
  if (nAssociatedStasPerBss[bss] == n)
    {
      // All STAs of this BSS are associated, we can extend beacon interval if bianchi flag is enabled
      Ptr<WifiNetDevice> apDevice = apDevices[bss].Get (0)->GetObject<WifiNetDevice> ();
      Ptr<ApWifiMac> apWifiMac = apDevice->GetMac ()->GetObject<ApWifiMac> ();
      apWifiMac->SetAttribute ("BeaconInterval", TimeValue (MicroSeconds (beaconInterval)));
    }
  if (filterOutNonAddbaEstablished)
    {
      // Here, we make sure that there is at least one packet in the queue
      // after association (paquets queued before are dropped)
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
BackoffTrace (std::string context, uint32_t newVal)
{
  // std::cout  <<"BO " <<Simulator::Now ().GetMicroSeconds () << " " << ContextToNodeId (context) << " " << newVal << std::endl;
}

void
CwTrace (std::string context, uint32_t oldVal, uint32_t newVal)
{
  // std::cout <<"CW "<< Simulator::Now ().GetMicroSeconds () << " " << ContextToNodeId (context) << " " << newVal << std::endl;
}

int
setWifiStandard (WifiHelper &wifi, std::string &dataRate, int32_t &freq, std::string standard, std::string mcs, int32_t bw)
{
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
  else if (standard == "11ac")
    {
      wifi.SetStandard (WIFI_STANDARD_80211ac);
      dataRate = "VhtMcs" + mcs;
      freq = 5170 + (bw / 2); //so as to have 5180/5190/5210/5250 for 20/40/80/160
      if (bw != 20 && bw != 40 && bw != 80 && bw != 160)
        {
          std::cout << "Bandwidth is not compatible with standard" << std::endl;
          return 1;
        }
    }
  else if (standard == "11ax_2_4GHZ")
    {
      wifi.SetStandard (WIFI_STANDARD_80211ax_2_4GHZ);
      dataRate = "HeMcs" + mcs;
      freq = 2402 + (bw / 2); //so as to have 2412/2422/2442 for 20/40/80
      if (bw != 20 && bw != 40 && bw != 80)
        {
          std::cout << "Bandwidth is not compatible with standard" << std::endl;
          return 1;
        }
    }
  else if (standard == "11ax_5GHZ")
    {
      wifi.SetStandard (WIFI_STANDARD_80211ax_5GHZ);
      dataRate = "HeMcs" + mcs;
      freq = 5170 + (bw / 2); //so as to have 5180/5190/5210/5250 for 20/40/80/160
      if (bw != 20 && bw != 40 && bw != 80 && bw != 160)
        {
          std::cout << "Bandwidth is not compatible with standard" << std::endl;
          return 1;
        }
    }
  else
    {
      std::cout << "Unknown OFDM standard (please refer to the listed possible values)" << std::endl;
      return 1;
    }

  return 0;
}

void
AddClient (ApplicationContainer &clientApps, Ipv4Address address, Ptr<Node> node, uint16_t port, Time interval, uint32_t payloadSize, double next_rng)
{
  UdpClientHelper client (address, port);

  if (filterOutNonAddbaEstablished)
    client.SetAttribute ("MaxPackets", UintegerValue (1u));
  else
    client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));

  client.SetAttribute ("Interval", TimeValue (interval + NanoSeconds (next_rng)));
  client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  clientApps.Add (client.Install (node));
}

void
AddServer (ApplicationContainer &serverApps, UdpServerHelper &server, Ptr<Node> node)
{
  serverApps.Add (server.Install (node));
}

int 
phySetup (SpectrumWifiPhyHelper &spectrumPhy, int32_t freq, int32_t bw)
{
  // path loss model uses one of the 802.11ax path loss models
  // described in the TGax simulations scenarios.
  // currently using just the IndoorPropagationLossModel, which
  // appears suitable for Test2 - Enterprise.
  // additional code tweaks needed for Test 1 and Test 3,
  // handling of 'W=1 wall' and using the ItuUmitPropagationLossModel
  // for Test 4.
  Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
  // more prominent example values:
  lossModel ->SetAttribute ("ReferenceDistance", DoubleValue (1.0));
  lossModel ->SetAttribute ("Exponent", DoubleValue (3.5));
  lossModel ->SetAttribute ("ReferenceLoss", DoubleValue (50));

  Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();

  Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
  spectrumChannel->AddPropagationLossModel (lossModel);  
  spectrumChannel->SetPropagationDelayModel (delayModel);

  spectrumPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  spectrumPhy.SetChannel (spectrumChannel);
  spectrumPhy.SetErrorRateModel ("ns3::YansErrorRateModel"); // YANS is more aligned with link simulations

  bool enableFrameCapture;
  GET_GLOB_VAL_G(BooleanValue, enableFrameCapture);
  if (enableFrameCapture)
    {
      spectrumPhy.SetFrameCaptureModel ("ns3::SimpleFrameCaptureModel");
    }
  
  bool enableThresholdPreambleDetection;
  GET_GLOB_VAL_G(BooleanValue, enableThresholdPreambleDetection);
  if (enableThresholdPreambleDetection)
    {
      spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel");
    }
  
  spectrumPhy.Set ("Frequency", UintegerValue (freq)); // channel 36 at 20 MHz
  spectrumPhy.Set ("ChannelWidth", UintegerValue (bw));

  return 0;
}

int
mobilitySetup (MobilityHelper &mobility, const double d, const double r)
{
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  UniformDiscPositionAllocator unitDiscPosAllocator;

  bool fixedPosition;
  GET_GLOB_VAL_G (BooleanValue, fixedPosition);
  std::cout << "fixedPosition: " << fixedPosition << std::endl;

  // consistent stream to that positional layout is not perturbed by other configuration choices
  int64_t stream = 100;

  for(uint32_t bss = 0; bss < nBss; bss++)
    {
      double theta = 2.0 * M_PI * (bss - 1) / 6.0;
      double x = bss ? d * cos(theta) : 0.0;
      double y = bss ? d * sin(theta) : 0.0;

      // AP
      positionAlloc->Add (Vector (x, y, 0.0));

      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.
      // To ensure unique randomness of positions, each allocator must be allocated a different stream number.
      unitDiscPosAllocator.AssignStreams (stream + bss);
      unitDiscPosAllocator.SetX (x);
      unitDiscPosAllocator.SetY (y);
      unitDiscPosAllocator.SetZ (0.0);
      unitDiscPosAllocator.SetRho (r);

      // std::cout << "[" << bss << "] theta: " << theta << ", x: " << x << ", y: " << y << ", stream: "<< (stream + bss) << std::endl;

      // std::cout << "    ";
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v;
          if (fixedPosition)
            v = Vector (x, y + r, 0.0);
          else
            v = unitDiscPosAllocator.GetNext ();
          // std::cout << i << " (" << v.x << ", " << v.y << ", " << v.z << ") ";
          // if(i % 5 == 4) std::cout << std::endl << "    ";
          positionAlloc->Add (v);
        }
      // std::cout << std::endl;
    }

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator (positionAlloc);
    mobility.Install (allNodes);

    return 0;
}

void
PopulateArpCache ()
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
    // NS_ASSERT(ip != 0);
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
    // NS_ASSERT(ip !=0);
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
      outFile << arrivals[i].m_time.GetSeconds () + arrivals[i].m_duration.GetSeconds () << " ";
      outFile << arrivals[i].m_duration.GetSeconds () * 1000.0 << " " << arrivals[i].m_power << std::endl;
    }
  outFile.close ();
  std::cout << "Spectrum Phy Stats written to: " << filename << std::endl;
}

void
SaveSpatialReuseStats (const std::string filename,
                       const double d,
                       const double r,
                       const int freqHz,
                       const double csr,
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

    bool verifyResults;
    GET_GLOB_VAL_G(BooleanValue, verifyResults);

  uint32_t numNodes = packetsReceived.size ();
  uint32_t n = (numNodes / nBss) - 1;

  outFile << "Spatial Reuse Statistics" << std::endl;
  outFile << "APs: " << nBss << std::endl;
  outFile << "Nodes per AP: " << n << std::endl;
  outFile << "Distance between APs [m]: " << d << std::endl;
  outFile << "Radius [m]: " << r << std::endl;

  std::cout << "Spatial Reuse Statistics" << std::endl;
  std::cout << "APs: " << nBss << std::endl;
  std::cout << "Nodes per AP: " << n << std::endl;
  std::cout << "Distance between APs [m]: " << d << std::endl;
  std::cout << "Radius [m]: " << r << std::endl;
  std::cout << "Uplink   [Mbps]: " << uplink << std::endl;
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

      uint32_t bssFirstStaIdx = (bss - 1) * n + bss;
      uint32_t bssLastStaIdx = bssFirstStaIdx + n;

      // std::cout << "[" << bss << "] bssFirstStaIdx: " << bssFirstStaIdx << ", bssLastStaIdx: " << bssLastStaIdx << std::endl;

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

      double maxExpectedThroughputBss1Down;
      double maxExpectedThroughputBss1Up;
      double minExpectedThroughputBss1Down;
      double minExpectedThroughputBss1Up;
      GET_GLOB_VAL_G(DoubleValue, maxExpectedThroughputBss1Down);
      GET_GLOB_VAL_G(DoubleValue, maxExpectedThroughputBss1Up);
      GET_GLOB_VAL_G(DoubleValue, minExpectedThroughputBss1Down);
      GET_GLOB_VAL_G(DoubleValue, minExpectedThroughputBss1Up);

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

          printf("AP|    Throughput   | Air-time  |\n");
          printf("  | Uplink |Downlink|Utilization|\n");
          printf("  | [Mbps] | [Mbps] |    [%%]    |\n");
          printf("--|--------|--------|-----------|\n");
        }

      outFile << "Throughput,  AP" << bss << " Uplink   [Mbps] : " << tputApUplink << std::endl;
      outFile << "Throughput,  AP" << bss << " Downlink [Mbps] : " << tputApDownlink << std::endl;

      double area = M_PI * r * r;
      outFile << "Area Capacity, AP" << bss << " Uplink   [Mbps/m^2] : " << tputApUplink / area << std::endl;
      outFile << "Area Capacity, AP" << bss << " Downlink [Mbps/m^2] : " << tputApDownlink / area << std::endl;

      outFile << "Spectrum Efficiency, AP" << bss << " Uplink   [Mbps/Hz] : " << tputApUplink / freqHz << std::endl;
      outFile << "Spectrum Efficiency, AP" << bss << " Downlink [Mbps/Hz] : " << tputApDownlink / freqHz << std::endl;

      double airtimeUtil = static_cast<double> (busyTime[bss - 1].GetNanoSeconds ()) / 1e9 / effectiveDuration;

      outFile << "Air-time utilization, AP" << bss << " [%] : " << (airtimeUtil * 100) << std::endl;

      // TODO: debug to print out t-put, can remove
      printf("%2d|%8.4f|%8.4f|%11.7f|\n", bss, tputApUplink, tputApDownlink, airtimeUtil * 100);
    }

  std::cout << "Total Throughput Uplink   [Mbps] : " << tputApUplinkTotal << std::endl;
  std::cout << "Total Throughput Downlink [Mbps] : " << tputApDownlinkTotal << std::endl;

  outFile << "Total Throughput Uplink   [Mbps] : " << tputApUplinkTotal << std::endl;
  outFile << "Total Throughput Downlink [Mbps] : " << tputApDownlinkTotal << std::endl;
  
  double rxThroughputPerNode[numNodes];
  double txThroughputPerNode[numNodes];
  // output for all nodes
  outFile << "Node, packetsReceived[pkts], bytesReceived[bytes], rxThroughputPerNode[Mb/s], bytesTransmitted[bytes], txThroughputPerNode[Mb/s]" << std::endl;
  for (uint32_t k = 0; k < numNodes; k++)
    {
      double bitsReceived = bytesReceived[k] * 8;
      double bitsTransmitted = bytesTransmitted[k] * 8;
      rxThroughputPerNode[k] = static_cast<double> (bitsReceived) / 1e6 / duration;
      txThroughputPerNode[k] = static_cast<double> (bitsTransmitted) / 1e6 / duration;
      outFile << k << ", " << packetsReceived[k] << ", " << bytesReceived[k] << ", " << rxThroughputPerNode[k] 
        << ", " << bytesTransmitted[k] << ", " << txThroughputPerNode[k] << std::endl;
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
  std::cout << "Udp Flow Monitor written to: " << filename << std::endl;
}

// main script
int
main (int argc, char *argv[])
{
  int ret_val;
  std::string outputFilePrefix ("spatial-reuse");
  std::string testname ("test");

  double obssPdThresholdBss[MAX_NBSS] = {
    -99.0, -99.0, -99.0, -99.0, -99.0, -99.0, -99.0
  };
  double obssPdThresholdMaxBss[MAX_NBSS] = {
    -62.0, -62.0, -62.0, -62.0, -62.0, -62.0, -62.0
  };
  double obssPdThresholdMinBss[MAX_NBSS] = {
    -82.0, -82.0, -82.0, -82.0, -82.0, -82.0, -82.0
  };

  bool bianchi;
  bool disableArp;
  bool enableAscii;
  bool enableObssPd;
  bool enablePcap;
  bool enableRts;
  bool performTgaxTimingChecks;
  bool powerBackoff;
  bool useExplicitBarAfterMissedBlockAck;
  bool useIdealWifiManager;

  double applicationTxStart;
  double ccaTrAp;
  double ccaTrSta;
  double d;
  double txRange;
  double powAp;
  double powSta;
  double r;
  double rxGain;
  double rxSensitivity;
  double txGain;
  double txStartOffset;

  int32_t bw;
  int32_t maxSlrc;

  std::string standard;
  std::string obssPdAlgorithm;

  uint16_t antennas;
  uint16_t colorBss[MAX_NBSS];
  uint16_t maxSupportedRxSpatialStreams;
  uint16_t maxSupportedTxSpatialStreams;
  uint16_t mcs;

  uint32_t maxMissedBeacons;
  uint32_t maxAmpduSizeBss[MAX_NBSS];
  uint32_t payloadSizeDownlink;
  uint32_t payloadSizeUplink;

  uint64_t maxQueueDelay;

  CommandLine cmd;
  // cmd.Parse (argc, argv);
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();
  // override input file default values via command line
  cmd.Parse (argc, argv);

  GET_GLOB_VAL_G (DoubleValue, aggregateDownlinkMbps);
  GET_GLOB_VAL_G (DoubleValue, aggregateUplinkMbps);
  GET_GLOB_VAL_G (UintegerValue, antennas);
  GET_GLOB_VAL_G (DoubleValue, applicationTxStart);
  GET_GLOB_VAL_G (UintegerValue, beaconInterval);
  GET_GLOB_VAL_G (BooleanValue, bianchi);
  GET_GLOB_VAL_G (IntegerValue, bw);
  GET_GLOB_VAL_G (DoubleValue, ccaTrAp);
  GET_GLOB_VAL_G (DoubleValue, ccaTrSta);
  GET_GLOB_VAL_G (DoubleValue, d);
  GET_GLOB_VAL_G (BooleanValue, disableArp);
  GET_GLOB_VAL_G (DoubleValue, duration);
  GET_GLOB_VAL_G (BooleanValue, enableAscii);
  GET_GLOB_VAL_G (BooleanValue, enableObssPd);
  GET_GLOB_VAL_G (BooleanValue, enablePcap);
  GET_GLOB_VAL_G (BooleanValue, enableRts);
  GET_GLOB_VAL_G (BooleanValue, filterOutNonAddbaEstablished);
  GET_GLOB_VAL_G (UintegerValue, maxMissedBeacons);
  GET_GLOB_VAL_G (UintegerValue, maxQueueDelay);
  GET_GLOB_VAL_G (IntegerValue, maxSlrc);
  GET_GLOB_VAL_G (UintegerValue, maxSupportedRxSpatialStreams);
  GET_GLOB_VAL_G (UintegerValue, maxSupportedTxSpatialStreams);
  GET_GLOB_VAL_G (UintegerValue, mcs);
  GET_GLOB_VAL_G (UintegerValue, n);
  GET_GLOB_VAL_G (UintegerValue, nBss);
  GET_GLOB_VAL_G (StringValue, obssPdAlgorithm);
  GET_GLOB_VAL_G (UintegerValue, payloadSizeDownlink);
  GET_GLOB_VAL_G (UintegerValue, payloadSizeUplink);
  GET_GLOB_VAL_G (BooleanValue, performTgaxTimingChecks);
  GET_GLOB_VAL_G (DoubleValue, powAp);
  GET_GLOB_VAL_G (BooleanValue, powerBackoff);
  GET_GLOB_VAL_G (DoubleValue, powSta);
  GET_GLOB_VAL_G (DoubleValue, r);
  GET_GLOB_VAL_G (DoubleValue, rxGain);
  GET_GLOB_VAL_G (DoubleValue, rxSensitivity);
  GET_GLOB_VAL_G (StringValue, standard);
  GET_GLOB_VAL_G (DoubleValue, txGain);
  GET_GLOB_VAL_G (DoubleValue, txRange);
  GET_GLOB_VAL_G (DoubleValue, txStartOffset);
  GET_GLOB_VAL_G (BooleanValue, useExplicitBarAfterMissedBlockAck);
  GET_GLOB_VAL_G (BooleanValue, useIdealWifiManager);

  GET_GLOB_VAL (UintegerValue, g_colorBss1, colorBss[0]);
  GET_GLOB_VAL (UintegerValue, g_colorBss2, colorBss[1]);
  GET_GLOB_VAL (UintegerValue, g_colorBss3, colorBss[2]);
  GET_GLOB_VAL (UintegerValue, g_colorBss4, colorBss[3]);
  GET_GLOB_VAL (UintegerValue, g_colorBss5, colorBss[4]);
  GET_GLOB_VAL (UintegerValue, g_colorBss6, colorBss[5]);
  GET_GLOB_VAL (UintegerValue, g_colorBss7, colorBss[6]);

  GET_GLOB_VAL (UintegerValue, g_maxAmpduSizeBss1, maxAmpduSizeBss[0]);
  GET_GLOB_VAL (UintegerValue, g_maxAmpduSizeBss2, maxAmpduSizeBss[1]);
  GET_GLOB_VAL (UintegerValue, g_maxAmpduSizeBss3, maxAmpduSizeBss[2]);
  GET_GLOB_VAL (UintegerValue, g_maxAmpduSizeBss4, maxAmpduSizeBss[3]);
  GET_GLOB_VAL (UintegerValue, g_maxAmpduSizeBss5, maxAmpduSizeBss[4]);
  GET_GLOB_VAL (UintegerValue, g_maxAmpduSizeBss6, maxAmpduSizeBss[5]);
  GET_GLOB_VAL (UintegerValue, g_maxAmpduSizeBss7, maxAmpduSizeBss[6]);

  if (!powerBackoff)
    {
      memcpy(obssPdThresholdMinBss, obssPdThresholdBss, sizeof(obssPdThresholdMinBss));
    }

  if (bianchi)
    {
      filterOutNonAddbaEstablished = false;
      useExplicitBarAfterMissedBlockAck = false;
      beaconInterval = 1024 * std::min<uint64_t> (ceil (duration * 1000 * 1000 / 1024), 65535); // beacon interval needs to be a multiple of time units (1024 us)
      maxMissedBeacons = 4294967295;
    }

  if (filterOutNonAddbaEstablished)
    {
      maxQueueDelay = duration * 1000; // make sure there is no MSDU lifetime expired
      disableArp = true;
    }
  
  if (enableRts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
    }

  uint32_t uMaxSlrc = maxSlrc < 0 ? std::numeric_limits<uint32_t>::max () : maxSlrc;

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

  std::ostringstream ossMcs;
  ossMcs << mcs;

  // CSR is a calculated value that is used for displaying the
  // estimated range in which an AP can successfully receive from STAs.
  // first, let us calculate the max distance, txRange, that a transmitting STA's signal
  // can be received above the CcaEdThreshold.
  // for simple calculation, we assume Friis propagation loss, CcaEdThreshold = -102dBm,
  // freq = 5.9GHz, no antenna gains, txPower = 10 dBm. the resulting value is:
  // calculation of CSR see: https://onlinelibrary.wiley.com/doi/pdf/10.1002/wcm.264
  // In (8), the optimum CSR = D x S0 ^ (1 / gamma)
  // S0 (the  min. SNR value for decoding a particular rate MCS) is calculated for several MCS values.
  // For reference, please see the  802.11ax Evaluation Methodology document (Appendix 3).
  // https://mentor.ieee.org/802.11/dcn/14/11-14-0571-12-00ax-evaluation-methodology.do
  // For the following conditions:
  // AWGN Channel
  // BCC with 1482 bytes Packet Length
  // PER=0.1
  // the minimum SINR values (S0) are
  //
  // [MCS S0]
  // [0 0.7dB]
  // [1 3.7dB]
  // [2 6.2dB]
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
  double csr = txRange * pow (s0, 1.0 / gamma); // carrier sense range

  // set Wifi standard
  WifiHelper wifi;
  std::string dataRate;
  int32_t freq;
  ret_val = setWifiStandard (wifi, dataRate, freq, standard, ossMcs.str (), bw);
  switch (ret_val)
    {
    case 1:
      std::cout << "Bandwidth is not compatible with standard" << std::endl;
      return 1;
    case 2:
      std::cout << "Unknown OFDM standard (please refer to the listed possible values)" << std::endl;
      return 2;
    default:
      std::cout << "Data rate: " << dataRate << ", frequency: " << freq << ", standard: " << standard << std::endl;
      break;
    }
  
  // disable ObssPd if not 11ax
  if ((standard != "11ax_2_4GHZ") && (standard != "11ax_5GHZ"))
    {
      enableObssPd = false;
    }
  
  // total expected nodes, n STAs for each AP.
  uint32_t numNodes = nBss * (n + 1);
  packetsReceived = std::vector<uint64_t> (numNodes, 0u);
  bytesReceived = std::vector<uint64_t> (numNodes, 0u);
  bytesTransmitted = std::vector<uint64_t> (numNodes, 0u);
  nAssociatedStasPerBss = std::vector<uint32_t> (nBss * n);
  
  busyTime = std::vector<Time> (nBss, Seconds (0));

  packetsReceivedPerNode.resize (numNodes, std::vector<uint64_t> (numNodes, 0));
  rssiPerNode.resize (numNodes, std::vector<double> (numNodes, 0.0));

  // When logging, use prefixes
  LogComponentEnableAll (LOG_PREFIX_TIME);
  LogComponentEnableAll (LOG_PREFIX_FUNC);
  LogComponentEnableAll (LOG_PREFIX_NODE);
  PacketMetadata::Enable ();

  // Create nodes and containers
  Ptr<Node> aps[nBss];
  NodeContainer stas[nBss], nodes[nBss]; // node containers for two APs and their STAs

  for (uint32_t bss = 0; bss < nBss; bss++)
    {
      aps[bss] = CreateObject<Node> ();
      for (uint32_t i = 0; i < n; i++)
        {
          Ptr<Node> sta = CreateObject<Node> ();
          stas[bss].Add (sta);
        }
      // AP at front of node container, then STAs
      nodes[bss].Add (aps[bss]);
      nodes[bss].Add (stas[bss]);
      // container for all nodes
      allNodes.Add(nodes[bss]);
    }

  // PHY setup
  SpectrumWifiPhyHelper spectrumPhy;
  ret_val = phySetup(spectrumPhy, freq, bw);

  if (useIdealWifiManager)
    {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
    }
  else
    {
      std::string ctrlRate;

      if ((standard == "11n_2_4GHZ") || (standard == "11ax_2_4GHZ"))
        ctrlRate = "ErpOfdmRate";
      else
       ctrlRate = "OfdmRate";

      if (mcs == 0)
        ctrlRate += "6Mbps";
      else if (mcs < 3)
        ctrlRate += "12Mbps";
      else
        ctrlRate += "24Mbps";

      std::cout << "Control Rate: " << ctrlRate << std::endl;
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (dataRate), "ControlMode", StringValue(ctrlRate));
    }
  
  // WiFi setup / helpers
  WifiMacHelper mac;

  char ssid_char;
  int64_t wifiStream = 700;

  for (uint32_t bss = 0; bss < nBss; bss++)
    {
      // Set PHY power and CCA threshold for STAs
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta));
      spectrumPhy.Set ("TxGain", DoubleValue (txGain));
      spectrumPhy.Set ("RxGain", DoubleValue (rxGain));
      spectrumPhy.Set ("Antennas", UintegerValue (antennas));
      spectrumPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (maxSupportedTxSpatialStreams));
      spectrumPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (maxSupportedRxSpatialStreams));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrSta));
      spectrumPhy.Set ("RxSensitivity", DoubleValue (rxSensitivity));

      if (enableObssPd)
      {
        wifi.SetObssPdAlgorithm ("ns3::" + obssPdAlgorithm,
                                "ObssPdLevelMin", DoubleValue (obssPdThresholdMinBss[bss]),
                                "ObssPdLevelMax", DoubleValue (obssPdThresholdMaxBss[bss]),
                                "ObssPdLevel", DoubleValue (obssPdThresholdBss[bss]),
                                "d", DoubleValue (d),
                                "r", DoubleValue (r),
                                "mcs", UintegerValue (mcs));
      }

      ssid_char = 'A' + bss;
      Ssid ssid = Ssid (std::string(1, ssid_char));
      int64_t streamId = wifiStream + 2 * bss;

      // std::cout << "[" << bss << "] SSID: " << ssid << ", stream: " << streamId << ", BssColor: " << colorBss[bss] << std::endl;

      mac.SetType ("ns3::StaWifiMac", "MaxMissedBeacons", UintegerValue (maxMissedBeacons), "Ssid", SsidValue (ssid));
      staDevices[bss] = wifi.Install (spectrumPhy, mac, stas[bss]);
      wifi.AssignStreams (staDevices[bss], streamId);

      // Set PHY power and CCA threshold for APs
      spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
      spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaTrAp));

      mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
      apDevices[bss] = wifi.Install (spectrumPhy, mac, aps[bss]);
      wifi.AssignStreams (apDevices[bss], streamId + 1);
      Ptr<WifiNetDevice> apDevice = apDevices[bss].Get (0)->GetObject<WifiNetDevice> ();
      Ptr<ApWifiMac> apWifiMac = apDevice->GetMac ()->GetObject<ApWifiMac> ();

      // The below statements may be simplified in a future HeConfigurationHelper
      if (enableObssPd)
        {
          Ptr<HeConfiguration> heConfiguration = apDevice->GetHeConfiguration ();
          heConfiguration->SetAttribute ("BssColor", UintegerValue (colorBss[bss]));
        }

    }

  // Assign positions to all nodes using position allocator
  MobilityHelper mobility;
  mobilitySetup (mobility, d, r);

  double perNodeUplinkMbps = aggregateUplinkMbps / n;
  double perNodeDownlinkMbps = aggregateDownlinkMbps / n;
  Time intervalUplink = MicroSeconds (payloadSizeUplink * 8 / perNodeUplinkMbps);
  Time intervalDownlink = MicroSeconds (payloadSizeDownlink * 8 / perNodeDownlinkMbps);
  std::cout << "Uplink interval: " << intervalUplink << ", Downlink interval: " << intervalDownlink << std::endl;
  std::cout << "ApplicationTxStart: " << applicationTxStart << ", Duration: " << duration << std::endl;
  std::cout << "nBss: " << nBss << ", nStas/Bss: " << n << " => nStas: " << n * nBss << std::endl;

  Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable> ();
  // assign stream to prevent perturbations
  urv->SetAttribute ("Stream", IntegerValue (200));
  urv->SetAttribute ("Min", DoubleValue (-txStartOffset));
  urv->SetAttribute ("Max", DoubleValue (txStartOffset));

  /* Internet stack */
  InternetStackHelper stack;

  uint64_t stackStream = 900;
  for(uint32_t bss = 0; bss < nBss; bss++)
    {
      stack.Install (nodes[bss]);
      stack.AssignStreams (nodes[bss], stackStream + bss);
    }

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer StaInterface[nBss];
  Ipv4InterfaceContainer ApInterface[nBss];

  address.SetBase ("192.168.1.0", "255.255.255.0");
  for(uint32_t bss = 0; bss < nBss; bss++)
    {
      StaInterface[bss] = address.Assign (staDevices[bss]);
      ApInterface[bss] = address.Assign (apDevices[bss]);

      // additional address help to prevent address overflow
      if(bss == 3)
        {
          // std::cout << "Extend ip to 192.168.2.0" << std::endl;
          address.SetBase ("192.168.2.0", "255.255.255.0");
        }
    }
  
  /* Setting applications */
  UdpServerHelper uplinkServers[nBss];
  UdpServerHelper downlinkServers[nBss];

  uint16_t uplinkPortBase = 9;
  uint16_t downlinkPortBase = 10;

  for(uint32_t bss = 0; bss < nBss; bss++)
    {
      uplinkServers[bss] = UdpServerHelper(uplinkPortBase + 2 * bss);
      downlinkServers[bss] = UdpServerHelper(downlinkPortBase + 2 * bss);
    }

  std::cout << "payloadSizeUplink: " << payloadSizeUplink << ", payloadSizeDownlink: " << payloadSizeDownlink << std::endl;
  if ((payloadSizeUplink > 0) || (payloadSizeDownlink > 0))
    {
      for (uint32_t bss = 0; bss < nBss; bss++)
        {
          // one network
          for (uint32_t i = 0; i < n; i++)
            {
              if (aggregateUplinkMbps > 0)
                {
                  double next_rng = txStartOffset > 0 ? urv->GetValue() : 0;
                  AddClient (uplinkClientApps, ApInterface[bss].GetAddress (0), stas[bss].Get (i), uplinkPortBase + 2 * bss, intervalUplink, payloadSizeUplink, next_rng);
                }
              if (aggregateDownlinkMbps > 0)
                {
                  double next_rng = txStartOffset > 0 ? urv->GetValue() : 0;
                  AddClient (downlinkClientApps, StaInterface[bss].GetAddress (i), aps[bss], downlinkPortBase + 2 * bss, intervalDownlink, payloadSizeDownlink, next_rng);
                  AddServer (downlinkServerApps, downlinkServers[bss], stas[bss].Get (i));
                }
            }
          if (aggregateUplinkMbps > 0)
            {
              AddServer (uplinkServerApps, uplinkServers[bss], aps[bss]);
            }
        }
    }

  FlowMonitorHelper flowMonHelperA;
  Ptr<FlowMonitor> monitorA = flowMonHelperA.Install (nodes[0]); // node A
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

  for (uint32_t i = 0; i < numNodes; i++)
    {
      uint32_t bss = i / (n + 1);
      
      std::stringstream stmp;
      stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
      Config::Set (stmp.str(), UintegerValue (std::min (maxAmpduSizeBss[bss], 4194303u)));
      // TODO:
      // BK_MaxAmpduSize
      // VO_MaxAmpduSize
      // VI_MaxAmpduSize
    }

  // Config::Connect ("/NodeList/*/DeviceList/*/Phy/MonitorSnifferRx", MakeCallback (&MonitorSniffRx));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback (&PacketTx));
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpServer/RxWithAddresses", MakeCallback (&PacketRx));
  // Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/ObssPdAlgorithm/Reset", MakeCallback (&PhyReset));

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

  g_stateFile.open (outputFilePrefix + "-state-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
  g_stateFile.setf (std::ios_base::fixed);
  g_stateFile << "NodeId Start Duration State" << std::endl;

  g_rxSniffFile.open (outputFilePrefix + "-rx-sniff-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
  g_rxSniffFile.setf (std::ios_base::fixed);
  g_rxSniffFile << "RxNodeId, DstNodeId, SrcNodeId, RxNodeAddr, DA, SA, Noise, Signal" << std::endl;

  g_txPowerFile.open (outputFilePrefix + "-tx-power-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
  g_txPowerFile.setf (std::ios_base::fixed);
  g_txPowerFile << "RxNodeId, DstNodeId, TxPowerDbm" << std::endl;

  if (enableObssPd)
    {
      g_phyResetFile.open (outputFilePrefix + "-phy-reset-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
      g_phyResetFile.setf (std::ios_base::fixed);
      g_phyResetFile << "Time, NodeId, BssColor, RssiDbm, TxPowerMaxDbmSiso, txPowerMaxDbmMimo" << std::endl;
    }

  g_TGaxCalibrationTimingsFile.open (outputFilePrefix + "-tgax-calibration-timings-" + testname + ".dat", std::ofstream::out | std::ofstream::trunc);
  g_TGaxCalibrationTimingsFile.setf (std::ios_base::fixed);

  // Logs
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::SpectrumWifiPhy/SignalArrival", MakeCallback (&SignalCb));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback (&StateCb));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/BlockAckManager/AgreementState", MakeCallback (&AddbaStateCb));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback (&StaAssocCb));

  // Trace backoff evolution
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/BackoffTrace", MakeCallback (&BackoffTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/CwTrace", MakeCallback (&CwTrace));

  // Save attribute configuration
  Config::SetDefault ("ns3::ConfigStore::Filename", StringValue (outputFilePrefix + ".config"));
  Config::SetDefault ("ns3::ConfigStore::FileFormat", StringValue ("RawText"));
  Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Save"));
  ConfigStore outputConfig;
  outputConfig.ConfigureAttributes ();

  if (enablePcap)
    {
      spectrumPhy.EnablePcap ("STA_pcap", staDevices[0]);
      spectrumPhy.EnablePcap ("AP_pcap", apDevices[0]);
    }

  if (disableArp)
    {
      PopulateArpCache ();
    }

  Time durationTime = Seconds (duration + applicationTxStart);
  Simulator::Stop (durationTime + Seconds (100)); // protection to make sure simulation finishes oneday
  Simulator::Run ();

  // run finished
  Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::SpectrumWifiPhy/SignalArrival", MakeCallback (&SignalCb));
  Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback (&StateCb));
  Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_Txop/BlockAckManager/AgreementState", MakeCallback (&AddbaStateCb));
  Config::Disconnect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback (&StaAssocCb));
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
  SaveSpatialReuseStats (outputFilePrefix + "-SR-stats-" + testname + ".dat", d,  r, freq, csr,  aggregateUplinkMbps, aggregateDownlinkMbps);

  // save flow-monitor results
  std::stringstream stmp;
  stmp << outputFilePrefix + "-A-" + testname + ".flowmon";

  if (monitorA != 0)
    {
      monitorA->SerializeToXmlFile (stmp.str ().c_str (), true, true);
    }

  SaveUdpFlowMonitorStats (outputFilePrefix + "-operatorA-" + testname, "simulationParams", monitorA, flowMonHelperA, durationTime.GetSeconds ());

  Simulator::Destroy ();
  return 0;
}
