/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 The Boeing Company
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
 * Author: Gary Pei <guangyu.pei@boeing.com>
 *
 * Updated by Tom Henderson, Rohan Patidar, Hao Yin and Sébastien Deronne
 */

// This program conducts a Bianchi analysis of a wifi network.
// It currently only supports 11a/b/g, and will be later extended
// to support 11n/ac/ax, including frame aggregation settings.

#include <fstream>
#include <math.h>       
#include <algorithm>
#include "ns3/log.h"
#include "ns3/config.h"
#include "ns3/gnuplot.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
#include "ns3/command-line.h"
#include "ns3/node-list.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/queue-size.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/mobility-helper.h"
#include "ns3/wifi-net-device.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-server.h"
#include "ns3/application-container.h"
#include "ns3/ampdu-subframe-header.h"
#include "ns3/wifi-mac.h"

#define PI 3.1415926535

NS_LOG_COMPONENT_DEFINE ("WifiBianchi");

using namespace ns3;

std::ofstream cwTraceFile;         ///< File that traces CW over time
std::ofstream backoffTraceFile;    ///< File that traces backoff over time
std::ofstream phyTxTraceFile;      ///< File that traces PHY transmissions  over time
std::ofstream macTxTraceFile;      ///< File that traces MAC transmissions  over time
std::ofstream macRxTraceFile;      ///< File that traces MAC receptions  over time
std::ofstream socketSendTraceFile; ///< File that traces packets transmitted by the application  over time
std::ofstream snrTraceFile;

std::map<Mac48Address, uint64_t> packetsReceived;              ///< Map that stores the total packets received per STA (and addressed to that STA)
std::map<Mac48Address, uint64_t> bytesReceived;                ///< Map that stores the total bytes received per STA (and addressed to that STA)
std::map<Mac48Address, uint64_t> packetsTransmitted;           ///< Map that stores the total packets transmitted per STA
std::map<Mac48Address, uint64_t> psduFailed;                   ///< Map that stores the total number of unsuccessfully received PSDUS (for which the PHY header was successfully received)  per STA (including PSDUs not addressed to that STA)
std::map<Mac48Address, uint64_t> psduSucceeded;                ///< Map that stores the total number of successfully received PSDUs per STA (including PSDUs not addressed to that STA)
std::map<Mac48Address, uint64_t> phyHeaderFailed;              ///< Map that stores the total number of unsuccessfully received PHY headers per STA
std::map<Mac48Address, uint64_t> rxEventWhileTxing;            ///< Map that stores the number of reception events per STA that occured while PHY was already transmitting a PPDU
std::map<Mac48Address, uint64_t> rxEventWhileRxing;            ///< Map that stores the number of reception events per STA that occured while PHY was already receiving a PPDU
std::map<Mac48Address, uint64_t> rxEventWhileDecodingPreamble; ///< Map that stores the number of reception events per STA that occured while PHY was already decoding a preamble
std::map<Mac48Address, uint64_t> rxEventAbortedByTx;           ///< Map that stores the number of reception events aborted per STA because the PHY has started to transmit

std::map<Mac48Address, Time> timeFirstReceived;    ///< Map that stores the time at which the first packet was received per STA (and the packet is addressed to that STA)
std::map<Mac48Address, Time> timeLastReceived;     ///< Map that stores the time at which the last packet was received per STA (and the packet is addressed to that STA)
std::map<Mac48Address, Time> timeFirstTransmitted; ///< Map that stores the time at which the first packet was transmitted per STA
std::map<Mac48Address, Time> timeLastTransmitted;  ///< Map that stores the time at which the last packet was transmitted per STA

std::map<double, uint64_t> snrMap;
std::set<uint32_t> associated; ///< Contains the IDs of the STAs that successfully associated to the access point (in infrastructure mode only)

bool tracing = false;    ///< Flag to enable/disable generation of tracing files
uint32_t pktSize = 1500; ///< packet size used for the simulation (in bytes)
uint8_t maxMpdus = 0;    ///< The maximum number of MPDUs in A-MPDUs (0 to disable MPDU aggregation)

std::map<std::string /* mode */, std::map<unsigned int /* number of nodes */, double /* calculated throughput */> > bianchiResultsEifs =
{
    /* 11ax */
    {"HeMcs0", {
        {5, 6.4019},
        {10, 5.8760},
        {15, 5.5782},
        {20, 5.3684},
        {25, 5.2047},
        {30, 5.0696},
        {35, 4.9537},
        {40, 4.8522},
        {45, 4.7613},
        {50, 4.6788},
    }},
    {"HeMcs1", {
        {5, 11.7116},
        {10, 10.7867},
        {15, 10.2542},
        {20, 9.8767},
        {25, 9.5810},
        {30, 9.3364},
        {35, 9.1262},
        {40, 8.9417},
        {45, 8.7764},
        {50, 8.6262},
    }},
    {"HeMcs2", {
        {5, 15.9567},
        {10, 14.7372},
        {15, 14.0254},
        {20, 13.5180},
        {25, 13.1194},
        {30, 12.7889},
        {35, 12.5045},
        {40, 12.2546},
        {45, 12.0305},
        {50, 11.8266},
    }},
    {"HeMcs3", {
        {5, 19.7457},
        {10, 18.2820},
        {15, 17.4163},
        {20, 16.7963},
        {25, 16.3078},
        {30, 15.9021},
        {35, 15.5524},
        {40, 15.2449},
        {45, 14.9687},
        {50, 14.7173},
    }},
    {"HeMcs4", {
        {5, 25.8947},
        {10, 24.0721},
        {15, 22.9698},
        {20, 22.1738},
        {25, 21.5437},
        {30, 21.0186},
        {35, 20.5650},
        {40, 20.1654},
        {45, 19.8059},
        {50, 19.4784},
    }},
    {"HeMcs5", {
        {5, 30.0542},
        {10, 28.0155},
        {15, 26.7625},
        {20, 25.8523},
        {25, 25.1295},
        {30, 24.5258},
        {35, 24.0034},
        {40, 23.5426},
        {45, 23.1277},
        {50, 22.7492},
    }},
    {"HeMcs6", {
        {5, 32.6789},
        {10, 30.5150},
        {15, 29.1708},
        {20, 28.1907},
        {25, 27.4107},
        {30, 26.7583},
        {35, 26.1931},
        {40, 25.6941},
        {45, 25.2446},
        {50, 24.8343},
    }},
    {"HeMcs7", {
        {5, 34.1710},
        {10, 31.9398},
        {15, 30.5451},
        {20, 29.5261},
        {25, 28.7140},
        {30, 28.0342},
        {35, 27.4449},
        {40, 26.9245},
        {45, 26.4554},
        {50, 26.0271},
    }},
    {"HeMcs8", {
        {5, 37.6051},
        {10, 35.2296},
        {15, 33.7228},
        {20, 32.6160},
        {25, 31.7314},
        {30, 30.9895},
        {35, 30.3455},
        {40, 29.7760},
        {45, 29.2623},
        {50, 28.7929},
    }},
    {"HeMcs9", {
        {5, 39.5947},
        {10, 37.1424},
        {15, 35.5731},
        {20, 34.4169},
        {25, 33.4911},
        {30, 32.7138},
        {35, 32.0385},
        {40, 31.4410},
        {45, 30.9016},
        {50, 30.4086},
    }},
    {"HeMcs10", {
        {5, 39.5947},
        {10, 37.1424},
        {15, 35.5731},
        {20, 34.4169},
        {25, 33.4911},
        {30, 32.7138},
        {35, 32.0385},
        {40, 31.4410},
        {45, 30.9016},
        {50, 30.4086},
    }},
    {"HeMcs11", {
        {5, 41.8065},
        {10, 39.2749},
        {15, 37.6383},
        {20, 36.4282},
        {25, 35.4575},
        {30, 34.6414},
        {35, 33.9316},
        {40, 33.3031},
        {45, 32.7355},
        {50, 32.2164},
    }},
};

std::map<std::string /* mode */, std::map<unsigned int /* number of nodes */, double /* calculated throughput */> > bianchiResultsDifs =
{
/* 11ax */
    {"HeMcs0", {
        {5, 6.4293},
        {10, 5.9133},
        {15, 5.6200},
        {20, 5.4130},
        {25, 5.2512},
        {30, 5.1176},
        {35, 5.0028},
        {40, 4.9021},
        {45, 4.8120},
        {50, 4.7301},
    }},
    {"HeMcs1", {
        {5, 11.8037},
        {10, 10.9131},
        {15, 10.3965},
        {20, 10.0288},
        {25, 9.7399},
        {30, 9.5004},
        {35, 9.2941},
        {40, 9.1129},
        {45, 8.9502},
        {50, 8.8021},
    }},
    {"HeMcs2", {
        {5, 16.1281},
        {10, 14.9742},
        {15, 14.2930},
        {20, 13.8045},
        {25, 13.4192},
        {30, 13.0986},
        {35, 12.8219},
        {40, 12.5784},
        {45, 12.3594},
        {50, 12.1598},
    }},
    {"HeMcs3", {
        {5, 20.0089},
        {10, 18.6480},
        {15, 17.8309},
        {20, 17.2410},
        {25, 16.7736},
        {30, 16.3837},
        {35, 16.0465},
        {40, 15.7491},
        {45, 15.4813},
        {50, 15.2369},
    }},
    {"HeMcs4", {
        {5, 26.3492},
        {10, 24.7107},
        {15, 23.6964},
        {20, 22.9553},
        {25, 22.3640},
        {30, 21.8683},
        {35, 21.4379},
        {40, 21.0571},
        {45, 20.7134},
        {50, 20.3991},
    }},
    {"HeMcs5", {
        {5, 30.6683},
        {10, 28.8843},
        {15, 27.7540},
        {20, 26.9210},
        {25, 26.2528},
        {30, 25.6906},
        {35, 25.2012},
        {40, 24.7671},
        {45, 24.3746},
        {50, 24.0151},
    }},
    {"HeMcs6", {
        {5, 33.4062},
        {10, 31.5485},
        {15, 30.3527},
        {20, 29.4662},
        {25, 28.7527},
        {30, 28.1508},
        {35, 27.6259},
        {40, 27.1597},
        {45, 26.7376},
        {50, 26.3507},
    }},
    {"HeMcs7", {
        {5, 34.9671},
        {10, 33.0739},
        {15, 31.8436},
        {20, 30.9282},
        {25, 30.1900},
        {30, 29.5665},
        {35, 29.0221},
        {40, 28.5382},
        {45, 28.0997},
        {50, 27.6975},
    }},
    {"HeMcs8", {
        {5, 38.5714},
        {10, 36.6144},
        {15, 35.3124},
        {20, 34.3355},
        {25, 33.5438},
        {30, 32.8728},
        {35, 32.2854},
        {40, 31.7623},
        {45, 31.2874},
        {50, 30.8512},
    }},
    {"HeMcs9", {
        {5, 40.6674},
        {10, 38.6851},
        {15, 37.3466},
        {20, 36.3371},
        {25, 35.5165},
        {30, 34.8197},
        {35, 34.2087},
        {40, 33.6638},
        {45, 33.1688},
        {50, 32.7137},
    }},
    {"HeMcs10", {
        {5, 40.6674},
        {10, 38.6851},
        {15, 37.3466},
        {20, 36.3371},
        {25, 35.5165},
        {30, 34.8197},
        {35, 34.2087},
        {40, 33.6638},
        {45, 33.1688},
        {50, 32.7137},
    }},
    {"HeMcs11", {
        {5, 43.0043},
        {10, 41.0039},
        {15, 39.6294},
        {20, 38.5865},
        {25, 37.7358},
        {30, 37.0116},
        {35, 36.3756},
        {40, 35.8076},
        {45, 35.2909},
        {50, 34.8154},
    }},
};

// Parse context strings of the form "/NodeList/x/DeviceList/x/..." to extract the NodeId integer
uint32_t
ContextToNodeId (std::string context)
{
  std::string sub = context.substr (10);
  uint32_t pos = sub.find ("/Device");
  return atoi (sub.substr (0, pos).c_str ());
}

// Parse context strings of the form "/NodeList/x/DeviceList/x/..." and fetch the Mac address
Mac48Address
ContextToMac (std::string context)
{
  std::string sub = context.substr (10);
  uint32_t pos = sub.find ("/Device");
  uint32_t nodeId = atoi (sub.substr (0, pos).c_str ());
  Ptr<Node> n = NodeList::GetNode (nodeId);
  Ptr<WifiNetDevice> d;
  for (uint32_t i = 0; i < n->GetNDevices (); i++)
    {
      d = n->GetDevice (i)->GetObject<WifiNetDevice> ();
      if (d)
        {
          break;
        }
    }
  return Mac48Address::ConvertFrom (d->GetAddress ());
}

// Functions for tracing.

void
IncrementCounter (std::map<Mac48Address, uint64_t> & counter, Mac48Address addr, uint64_t increment = 1)
{
  auto it = counter.find (addr);
  if (it != counter.end ())
   {
     it->second += increment;
   }
  else
   {
     counter.insert (std::make_pair (addr, increment));
   }
}

void
TracePacketReception (std::string context, Ptr<const Packet> p, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise, uint16_t staId)
{
  Ptr<Packet> packet = p->Copy ();
  if (txVector.IsAggregation ())
    {
      AmpduSubframeHeader subHdr;
      uint32_t extractedLength;
      packet->RemoveHeader (subHdr);
      extractedLength = subHdr.GetLength ();
      packet = packet->CreateFragment (0, static_cast<uint32_t> (extractedLength));
    }
  WifiMacHeader hdr;
  packet->PeekHeader (hdr);
  // hdr.GetAddr1() is the receiving MAC address
  if (hdr.GetAddr1 () != ContextToMac (context))
    { 
      return;
    }
  // hdr.GetAddr2() is the sending MAC address
  if (packet->GetSize () >= pktSize) // ignore non-data frames
    {
      IncrementCounter (packetsReceived, hdr.GetAddr2 ());
      IncrementCounter (bytesReceived, hdr.GetAddr2 (), pktSize);
      auto itTimeFirstReceived = timeFirstReceived.find (hdr.GetAddr2 ());
      if (itTimeFirstReceived == timeFirstReceived.end ())
        {
          timeFirstReceived.insert (std::make_pair (hdr.GetAddr2 (), Simulator::Now ()));
        }
      auto itTimeLastReceived = timeLastReceived.find (hdr.GetAddr2 ());
      if (itTimeLastReceived != timeLastReceived.end ())
        {
          itTimeLastReceived->second = Simulator::Now ();
        }
      else
        {
          timeLastReceived.insert (std::make_pair (hdr.GetAddr2 (), Simulator::Now ()));
        }
    }
}

void
CwTrace (std::string context, uint32_t oldVal, uint32_t newVal)
{
  NS_LOG_INFO ("CW time=" << Simulator::Now () << " node=" << ContextToNodeId (context) << " val=" << newVal);
  if (tracing)
    {
      cwTraceFile << Simulator::Now ().GetSeconds () << " " << ContextToNodeId (context) << " " << newVal << std::endl;
    }
}

void
BackoffTrace (std::string context, uint32_t newVal)
{
  NS_LOG_INFO ("Backoff time=" << Simulator::Now () << " node=" << ContextToNodeId (context) << " val=" << newVal);
  if (tracing)
    {
      backoffTraceFile << Simulator::Now ().GetSeconds () << " " << ContextToNodeId (context) << " " << newVal << std::endl;
    }
}

void
PhyRxTrace (std::string context, Ptr<const Packet> p, RxPowerWattPerChannelBand power)
{
  NS_LOG_INFO ("PHY-RX-START time=" << Simulator::Now () << " node=" << ContextToNodeId (context) << " size=" << p->GetSize ());
}

void
PhyRxPayloadTrace (std::string context, WifiTxVector txVector, Time psduDuration)
{
  NS_LOG_INFO ("PHY-RX-PAYLOAD-START time=" << Simulator::Now () << " node=" << ContextToNodeId (context) << " psduDuration=" << psduDuration);
}

void
PhyRxDropTrace (std::string context, Ptr<const Packet> p, WifiPhyRxfailureReason reason)
{
  NS_LOG_INFO ("PHY-RX-DROP time=" << Simulator::Now () << " node=" << ContextToNodeId (context) << " size=" << p->GetSize () << " reason=" << reason);
  Mac48Address addr = ContextToMac (context);
  switch (reason)
    {
    case UNSUPPORTED_SETTINGS:
      NS_FATAL_ERROR ("RX packet with unsupported settings!");
      break;
    case CHANNEL_SWITCHING:
      NS_FATAL_ERROR ("Channel is switching!");
      break;
    case BUSY_DECODING_PREAMBLE:
    {
      if (p->GetSize () >= pktSize) // ignore non-data frames
        {
          IncrementCounter (rxEventWhileDecodingPreamble, addr);
        }
      break;
    }
    case RXING:
    {
      if (p->GetSize () >= pktSize) // ignore non-data frames
        {
          IncrementCounter (rxEventWhileRxing, addr);
        }
      break;
    }
    case TXING:
    {
      if (p->GetSize () >= pktSize) // ignore non-data frames
        {
          IncrementCounter (rxEventWhileTxing, addr);
        }
      break;
    }
    case SLEEPING:
      NS_FATAL_ERROR ("Device is sleeping!");
      break;
    case PREAMBLE_DETECT_FAILURE:
      NS_FATAL_ERROR ("Preamble should always be detected!");
      break;
    case RECEPTION_ABORTED_BY_TX:
    {
      if (p->GetSize () >= pktSize) // ignore non-data frames
        {
          IncrementCounter (rxEventAbortedByTx, addr);
        }
      break;
    }
    case L_SIG_FAILURE:
    {
      if (p->GetSize () >= pktSize) // ignore non-data frames
        {
          IncrementCounter (phyHeaderFailed, addr);
        }
      break;
    }
    case SIG_A_FAILURE:
    //   NS_FATAL_ERROR ("Unexpected PHY header failure!");
      break;
    case PREAMBLE_DETECTION_PACKET_SWITCH:
      NS_FATAL_ERROR ("All devices should send with same power, so no packet switch during preamble detection should occur!");
      break;
    case FRAME_CAPTURE_PACKET_SWITCH:
      NS_FATAL_ERROR ("Frame capture should be disabled!");
      break;
    case OBSS_PD_CCA_RESET:
      NS_FATAL_ERROR ("Unexpected CCA reset!");
      break;
    case UNKNOWN:
    default:
      NS_FATAL_ERROR ("Unknown drop reason!");
      break;
    }
}

void
PhyRxDoneTrace (std::string context, Ptr<const Packet> p)
{
  NS_LOG_INFO ("PHY-RX-END time=" << Simulator::Now () << " node=" << ContextToNodeId (context) << " size=" << p->GetSize ());
}

void
PhyRxOkTrace (std::string context, Ptr<const Packet> p, double snr, WifiMode mode, WifiPreamble preamble)
{
  uint8_t nMpdus = (p->GetSize () / pktSize);
  snr = RatioToDb(snr);
  NS_LOG_INFO ("PHY-RX-OK time=" << Simulator::Now () << " node="
                                 << ContextToNodeId (context) << " size="
                                 << p->GetSize () << " nMPDUs="
                                 << p->GetSize () / pktSize << " snr="
                                 << snr << " mode="
                                 << mode << " preamble="
                                 << preamble);
  if ((maxMpdus != 0) && (nMpdus != 0) && (nMpdus != maxMpdus))
    {
      if (nMpdus > maxMpdus)
        {
          NS_FATAL_ERROR ("A-MPDU settings not properly applied: maximum configured MPDUs is " << +maxMpdus << " but received an A-MPDU containing " << +nMpdus << " MPDUs");
        }
      // NS_LOG_WARN ("Warning: less MPDUs aggregated in a received A-MPDU (" << +nMpdus << ") than configured (" << +maxMpdus << ")");
    }
  if (p->GetSize () >= pktSize) // ignore non-data frames
    {
      Mac48Address addr = ContextToMac (context);
      IncrementCounter (psduSucceeded, addr);
    }
  
  std::map<double, uint64_t>::iterator intr = snrMap.find(snr);
  if (intr != snrMap.end())
    {
      (*intr).second++;
    }
  else
    {
      snrMap.insert(std::pair<double, uint64_t>(snr, 1));
    }
}

void
PhyRxErrorTrace (std::string context, Ptr<const Packet> p, double snr)
{
  snr = RatioToDb(snr);
  NS_LOG_INFO ("PHY-RX-ERROR time=" << Simulator::Now () << " node=" << ContextToNodeId (context) << " size=" << p->GetSize () << " snr=" << snr);
  if (p->GetSize () >= pktSize) // ignore non-data frames
    {
      Mac48Address addr = ContextToMac (context);
      IncrementCounter (psduFailed, addr);
    }
  
  std::map<double, uint64_t>::iterator intr = snrMap.find(snr);
  if (intr != snrMap.end())
    {
      (*intr).second++;
    }
  else
    {
      snrMap.insert(std::pair<double, uint64_t>(snr, 1));
    }
}

void
PhyTxTrace (std::string context, Ptr<const Packet> p, double txPowerW)
{
  NS_LOG_INFO ("PHY-TX-START time=" << Simulator::Now () << " node=" << ContextToNodeId (context) << " size=" << p->GetSize () << " " << txPowerW);
  if (tracing)
    {
      phyTxTraceFile << Simulator::Now ().GetSeconds () << " " << ContextToNodeId (context) << " size=" << p->GetSize () << " " << txPowerW << std::endl;
    }
  if (p->GetSize () >= pktSize) // ignore non-data frames
    {
      Mac48Address addr = ContextToMac (context);
      IncrementCounter (packetsTransmitted, addr);
    }
}

void
PhyTxDoneTrace (std::string context, Ptr<const Packet> p)
{
  NS_LOG_INFO ("PHY-TX-END time=" << Simulator::Now () << " node=" << ContextToNodeId (context) << " " << p->GetSize ());
}

void
MacTxTrace (std::string context, Ptr<const Packet> p)
{
  if (tracing)
    {
      macTxTraceFile << Simulator::Now ().GetSeconds () << " " << ContextToNodeId (context) << " " << p->GetSize () << std::endl;
    }
}

void
MacRxTrace (std::string context, Ptr<const Packet> p)
{
  if (tracing)
    {
      macRxTraceFile << Simulator::Now ().GetSeconds () << " " << ContextToNodeId (context) << " " << p->GetSize () << std::endl;
    }
}

void
SocketSendTrace (std::string context, Ptr<const Packet> p, const Address &addr)
{
  if (tracing)
    {
      socketSendTraceFile << Simulator::Now ().GetSeconds () << " " << ContextToNodeId (context) << " " << p->GetSize () << " " << addr << std::endl;
    }
}

void
AssociationLog (std::string context, Mac48Address address)
{
  uint32_t nodeId = ContextToNodeId (context);
  auto it = associated.find (nodeId);
  if (it == associated.end ())
    {
      NS_LOG_DEBUG ("Association: time=" << Simulator::Now () << " node=" << nodeId);
      associated.insert (it, nodeId);
    }
  else
    {
      NS_FATAL_ERROR (nodeId << " is already associated!");
    }
}

void
DisassociationLog (std::string context, Mac48Address address)
{
  uint32_t nodeId = ContextToNodeId (context);
  NS_LOG_DEBUG ("Disassociation: time=" << Simulator::Now () << " node=" << nodeId);
  NS_FATAL_ERROR ("Device should not disassociate!");
}

void
RestartCalc ()
{
  bytesReceived.clear ();
  packetsReceived.clear ();
  packetsTransmitted.clear ();
  psduFailed.clear ();
  psduSucceeded.clear ();
  phyHeaderFailed.clear ();
  timeFirstReceived.clear ();
  timeLastReceived.clear ();
  rxEventWhileDecodingPreamble.clear ();
  rxEventWhileRxing.clear ();
  rxEventWhileTxing.clear ();
  rxEventAbortedByTx.clear ();
}

class Experiment
{
public:
  Experiment ();
  int Run (const WifiHelper &wifi, const YansWifiPhyHelper &wifiPhy, const WifiMacHelper &wifiMac, const YansWifiChannelHelper &wifiChannel,
           uint32_t trialNumber, uint32_t networkSize, double duration, bool pcap, bool infra, uint16_t channelWidth, uint16_t guardIntervalNs,
           double distance, double apTxPower, double staTxPower, uint16_t pktInterval);
};

Experiment::Experiment ()
{
}

int
Experiment::Run (const WifiHelper &helper, const YansWifiPhyHelper &wifiPhy, const WifiMacHelper &wifiMac, const YansWifiChannelHelper &wifiChannel,
                 uint32_t trialNumber, uint32_t networkSize, double duration, bool pcap, bool infra, uint16_t channelWidth, uint16_t guardIntervalNs,
                 double distance, double apTxPower, double staTxPower, uint16_t pktInterval)
{
  NodeContainer wifiNodes;
  if (infra)
    {
      wifiNodes.Create (networkSize + 1);
    }
  else
    {
      wifiNodes.Create (networkSize); 
    }

  YansWifiPhyHelper phy = wifiPhy;
  phy.SetChannel (wifiChannel.Create ());
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  WifiMacHelper mac = wifiMac;
  WifiHelper wifi = helper;
  NetDeviceContainer devices;
  uint32_t nNodes = wifiNodes.GetN ();
  if (infra)
    {
      Ssid ssid = Ssid ("wifi-bianchi");
      uint64_t beaconInterval = std::min<uint64_t> ((ceil ((duration * 1000000) / 1024) * 1024), (65535 * 1024)); //beacon interval needs to be a multiple of time units (1024 us)
      mac.SetType ("ns3::ApWifiMac",
                   "BeaconInterval", TimeValue (MicroSeconds (beaconInterval)),
                   "Ssid", SsidValue (ssid));
      phy.Set ("TxPowerStart", DoubleValue (apTxPower)); 
      phy.Set ("TxPowerEnd", DoubleValue (apTxPower));
      devices = wifi.Install (phy, mac, wifiNodes.Get (0));

      mac.SetType ("ns3::StaWifiMac",
                   "MaxMissedBeacons", UintegerValue (std::numeric_limits<uint32_t>::max ()),
                   "Ssid", SsidValue (ssid));
      phy.Set ("TxPowerStart", DoubleValue (staTxPower)); 
      phy.Set ("TxPowerEnd", DoubleValue (staTxPower));
      for (uint32_t i = 1; i < nNodes; ++i)
        {
          devices.Add (wifi.Install (phy, mac, wifiNodes.Get (i)));
        }
    }
  else
    {
      mac.SetType ("ns3::AdhocWifiMac");
      phy.Set ("TxPowerStart", DoubleValue (staTxPower));
      phy.Set ("TxPowerEnd", DoubleValue (staTxPower));
      devices = wifi.Install (phy, mac, wifiNodes);
    }

  wifi.AssignStreams (devices, trialNumber);

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (guardIntervalNs == 400 ? true : false));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/GuardInterval", TimeValue (NanoSeconds (guardIntervalNs)));

  // Configure aggregation
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      Ptr<NetDevice> dev = wifiNodes.Get (i)->GetDevice (0);
      Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
      wifi_dev->GetMac ()->SetAttribute ("BE_MaxAmpduSize", UintegerValue (maxMpdus * (pktSize + 50)));
      wifi_dev->GetMac ()->SetAttribute ("BK_MaxAmpduSize", UintegerValue (maxMpdus * (pktSize + 50)));
      wifi_dev->GetMac ()->SetAttribute ("VO_MaxAmpduSize", UintegerValue (maxMpdus * (pktSize + 50)));
      wifi_dev->GetMac ()->SetAttribute ("VI_MaxAmpduSize", UintegerValue (maxMpdus * (pktSize + 50)));
    }

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  // Set postion for AP
  positionAlloc->Add (Vector (1.0, 1.0, 0.0));

  // Set postion for STAs
  double angle = (static_cast<double> (10) / (nNodes - 2));
  for (uint32_t i = 0; i < (nNodes - 1); ++i)
    {
      positionAlloc->Add (Vector (1.0 + (distance * cos ((i * angle * PI) / 180)), 1.0 + (distance * sin ((i * angle * PI) / 180)), 0.0));
    }

  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (wifiNodes);

  PacketSocketHelper packetSocket;
  packetSocket.Install (wifiNodes);
  
  ApplicationContainer apps;
  Ptr<UniformRandomVariable> startTime = CreateObject<UniformRandomVariable> ();
  startTime->SetAttribute ("Stream", IntegerValue (trialNumber));
  startTime->SetAttribute ("Max", DoubleValue (5.0));

  uint32_t i = infra ? 1 : 0;
  for (; i < nNodes; ++i)
    {
      uint32_t j = infra ? 0 : (i + 1) % nNodes;
      PacketSocketAddress socketAddr;
      socketAddr.SetSingleDevice (devices.Get (i)->GetIfIndex ());
      socketAddr.SetPhysicalAddress (devices.Get (j)->GetAddress ());
      socketAddr.SetProtocol (1);

      Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient> ();
      client->SetRemote (socketAddr);
      wifiNodes.Get (i)->AddApplication (client);
      client->SetAttribute ("PacketSize", UintegerValue (pktSize));
      client->SetAttribute ("MaxPackets", UintegerValue (0));
      client->SetAttribute ("Interval", TimeValue (MicroSeconds (pktInterval)));
      double start = startTime->GetValue ();
      NS_LOG_DEBUG ("Client " << i << " starting at " << start);
      client->SetStartTime (Seconds (start));

      Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer> ();
      server->SetLocal (socketAddr);
      wifiNodes.Get (j)->AddApplication (server);
    }

  // Log packet receptions
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/MonitorSnifferRx", MakeCallback (&TracePacketReception));

  // Log association and disassociation
  if (infra)
    {
      Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback (&AssociationLog));
      Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc", MakeCallback (&DisassociationLog));
    }

  // Trace CW evolution
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Txop/CwTrace", MakeCallback (&CwTrace));
  // Trace backoff evolution
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Txop/BackoffTrace", MakeCallback (&BackoffTrace));
  // Trace PHY Tx start events
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/PhyTxBegin", MakeCallback (&PhyTxTrace));
  // Trace PHY Tx end events
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/PhyTxEnd", MakeCallback (&PhyTxDoneTrace));
  // Trace PHY Rx start events
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/PhyRxBegin", MakeCallback (&PhyRxTrace));
  // Trace PHY Rx payload start events
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/PhyRxPayloadBegin", MakeCallback (&PhyRxPayloadTrace));
  // Trace PHY Rx drop events
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/PhyRxDrop", MakeCallback (&PhyRxDropTrace));
  // Trace PHY Rx end events
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/PhyRxEnd", MakeCallback (&PhyRxDoneTrace));
  // Trace PHY Rx error events
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/State/RxError", MakeCallback (&PhyRxErrorTrace));
  // Trace PHY Rx success events
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/State/RxOk", MakeCallback (&PhyRxOkTrace));
  // Trace packet transmission by the device
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback (&MacTxTrace));
  // Trace packet receptions to the device
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback (&MacRxTrace));
  // Trace packets transmitted by the application
  Config::Connect ("/NodeList/*/$ns3::Node/ApplicationList/*/$ns3::PacketSocketClient/Tx", MakeCallback (&SocketSendTrace));

  Simulator::Schedule (Seconds (10), &RestartCalc);
  Simulator::Stop (Seconds (10 + duration));

  if (pcap)
    {
      phy.EnablePcap ("wifi_bianchi_pcap", devices);
    }

  Simulator::Run ();
  Simulator::Destroy ();

  if (tracing)
    {
      cwTraceFile.flush ();
      backoffTraceFile.flush ();
      phyTxTraceFile.flush ();
      macTxTraceFile.flush ();
      macRxTraceFile.flush ();
      socketSendTraceFile.flush ();
      snrTraceFile.flush ();
    }
    
  return 0;
}

uint64_t
GetCount (const std::map<Mac48Address, uint64_t> & counter, Mac48Address addr)
{
  uint64_t count = 0;
  auto it = counter.find (addr);
  if (it != counter.end ())
    {
      count = it->second;
    }
  return count;
}

int main (int argc, char *argv[])
{
  uint32_t nMinStas = 5;                  ///< Minimum number of STAs to start with
  uint32_t nMaxStas = 5;                  ///< Maximum number of STAs to end with
  uint32_t nStepSize = 5;                 ///< Number of stations to add at each step
  uint32_t verbose = 0;                   ///< verbosity level that increases the number of debugging traces
  double duration = 100;                  ///< duration (in seconds) of each simulation run (i.e. per trial and per number of stations)
  uint32_t trials = 1;                    ///< Number of runs per point in the plot
  bool pcap = false;                      ///< Flag to enable/disable PCAP files generation
  bool infra = false;                     ///< Flag to enable infrastructure model, ring adhoc network if not set
  std::string workDir = "./";             ///< the working directory to store generated files
  std::string phyMode = "OfdmRate54Mbps"; ///< the constant PHY mode used to transmit frames
  std::string standard ("11a");           ///< the 802.11 standard
  bool validate = false;                  ///< Flag used for regression in order to verify ns-3 results are in the expected boundaries
  uint8_t plotBianchiModel = 0x01;        ///< First bit corresponds to the DIFS model, second bit to the EIFS model
  double maxRelativeError = 0.015;        ///< Maximum relative error tolerated between ns-3 results and the Bianchi model (used for regression, i.e. when the validate flag is set)
  double frequency = 5;                   ///< The operating frequency band: 2.4, 5 or 6
  uint16_t channelWidth = 20;             ///< The constant channel width in MHz (only for 11n/ac/ax)
  uint16_t guardIntervalNs = 800;         ///< The guard interval in nanoseconds (800 or 400 for 11n/ac, 800 or 1600 or 3200 for 11 ax)
  uint16_t pktInterval = 1000;            ///< The socket packet interval in microseconds (a higher value is needed to reach saturation conditions as the channel bandwidth or the MCS increases)
  double distance = 0.001;                ///< The distance in meters between the AP and the STAs
  double apTxPower = 30;                  ///< The transmit power of the AP in dBm (if infrastructure only)
  double staTxPower = 30;                 ///< The transmit power of each STA in dBm (or all STAs if adhoc)

  // Disable fragmentation and RTS/CTS
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("22000"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("22000"));
  // Disable short retransmission failure (make retransmissions persistent)
  Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSlrc", UintegerValue (std::numeric_limits<uint32_t>::max ()));
  Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSsrc", UintegerValue (std::numeric_limits<uint32_t>::max ()));
  // Set maximum queue size to the largest value and set maximum queue delay to be larger than the simulation time
  Config::SetDefault ("ns3::WifiMacQueue::MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, std::numeric_limits<uint32_t>::max ())));
  Config::SetDefault ("ns3::WifiMacQueue::MaxDelay", TimeValue (Seconds (2 * duration)));
  Config::SetDefault ("ns3::QosTxop::UseExplicitBarAfterMissedBlockAck", BooleanValue (false));
  
  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "Logging level (0: no log - 1: simulation script logs - 2: all logs)", verbose);
  cmd.AddValue ("tracing", "Generate trace files", tracing);
  cmd.AddValue ("pktSize", "The packet size in bytes", pktSize);
  cmd.AddValue ("trials", "The maximal number of runs per network size", trials);
  cmd.AddValue ("duration", "Time duration for each trial in seconds", duration);
  cmd.AddValue ("pcap", "Enable/disable PCAP tracing", pcap);
  cmd.AddValue ("infra", "True to use infrastructure mode, false to use ring adhoc mode", infra);
  cmd.AddValue ("workDir", "The working directory used to store generated files", workDir);
  cmd.AddValue ("phyMode", "Set the constant PHY mode string used to transmit frames", phyMode);
  cmd.AddValue ("standard", "Set the standard (11a, 11b, 11g, 11n, 11ac, 11ax)", standard);
  cmd.AddValue ("nMinStas", "Minimum number of stations to start with", nMinStas);
  cmd.AddValue ("nMaxStas", "Maximum number of stations to start with", nMaxStas);
  cmd.AddValue ("nStepSize", "Number of stations to add at each step", nStepSize);
  cmd.AddValue ("plotBianchiModel", "First bit corresponds to the DIFS model, second bit to the EIFS model", plotBianchiModel);
  cmd.AddValue ("validate", "Enable/disable validation of the ns-3 simulations against the Bianchi model", validate);
  cmd.AddValue ("maxRelativeError", "The maximum relative error tolerated between ns-3 results and the Bianchi model (used for regression, i.e. when the validate flag is set)", maxRelativeError);
  cmd.AddValue ("frequency", "Set the operating frequency band: 2.4, 5 or 6", frequency);
  cmd.AddValue ("channelWidth", "Set the constant channel width in MHz (only for 11n/ac/ax)", channelWidth);
  cmd.AddValue ("guardIntervalNs", "Set the the guard interval in nanoseconds (800 or 400 for 11n/ac, 800 or 1600 or 3200 for 11 ax)", guardIntervalNs);
  cmd.AddValue ("maxMpdus", "Set the maximum number of MPDUs in A-MPDUs (0 to disable MPDU aggregation)", maxMpdus);
  cmd.AddValue ("distance", "Set the distance in meters between the AP and the STAs", distance);
  cmd.AddValue ("apTxPower", "Set the transmit power of the AP in dBm (if infrastructure only)", apTxPower);
  cmd.AddValue ("staTxPower", "Set the transmit power of each STA in dBm (or all STAs if adhoc)", staTxPower);
  cmd.AddValue ("pktInterval", "Set the socket packet interval in microseconds", pktInterval);
  cmd.Parse (argc, argv);

  if (tracing)
    {
      cwTraceFile.open ("wifi-bianchi-cw-trace.out");
      if (!cwTraceFile.is_open ())
        {
          NS_FATAL_ERROR ("Failed to open file wifi-bianchi-cw-trace.out");
        }
      backoffTraceFile.open ("wifi-bianchi-backoff-trace.out");
      if (!backoffTraceFile.is_open ())
        {
          NS_FATAL_ERROR ("Failed to open file wifi-bianchi-backoff-trace.out");
        }
      phyTxTraceFile.open ("wifi-bianchi-phy-tx-trace.out");
      if (!phyTxTraceFile.is_open ())
        {
          NS_FATAL_ERROR ("Failed to open file wifi-bianchi-phy-tx-trace.out");
        }
      macTxTraceFile.open ("wifi-bianchi-mac-tx-trace.out");
      if (!macTxTraceFile.is_open ())
        {
          NS_FATAL_ERROR ("Failed to open file wifi-bianchi-mac-tx-trace.out");
        }
      macRxTraceFile.open ("wifi-bianchi-mac-rx-trace.out");
      if (!macRxTraceFile.is_open ())
        {
          NS_FATAL_ERROR ("Failed to open file wifi-bianchi-mac-rx-trace.out");
        }
      socketSendTraceFile.open ("wifi-bianchi-socket-send-trace.out");
      if (!socketSendTraceFile.is_open ())
        {
          NS_FATAL_ERROR ("Failed to open file wifi-bianchi-socket-send-trace.out");
        }
      snrTraceFile.open ("wifi-bianchi-snr.out");
      if (!snrTraceFile.is_open ())
        {
          NS_FATAL_ERROR ("Failed to open file wifi-bianchi-snr.out");
        }
    }
  
  if (verbose >= 1)
    {
      LogComponentEnable ("WifiBianchi", LOG_LEVEL_ALL);
    }
  else
    {
      LogComponentEnable ("WifiBianchi", LOG_LEVEL_WARN);
    }
  if (verbose >= 2)
    {
      WifiHelper::EnableLogComponents ();
    }

  std::stringstream phyModeStr;
  phyModeStr << phyMode;
  if (phyMode.find ("Mcs") != std::string::npos)
    {
      phyModeStr << "_" << channelWidth << "MHz";
    }

  std::stringstream ss;
  ss << "wifi-"<< standard << "-p-" << pktSize << (infra ? "-infrastructure" : "-adhoc") << "-r-" << phyModeStr.str () << "-min-" << nMinStas << "-max-" << nMaxStas << "-step-" << nStepSize << "-throughput.plt";
  std::ofstream throughputPlot (ss.str ().c_str ());
  ss.str ("");
  ss << "wifi-" << standard << "-p-" << pktSize << (infra ? "-infrastructure" : "-adhoc") <<"-r-" << phyModeStr.str () << "-min-" << nMinStas << "-max-" << nMaxStas << "-step-" << nStepSize << "-throughput.eps";
  Gnuplot gnuplot = Gnuplot (ss.str ());

  WifiStandard wifiStandard;
  if (standard == "11a")
    {
      wifiStandard = WIFI_STANDARD_80211a;
      frequency = 5;
      channelWidth = 20;
    }
  else if (standard == "11b")
    {
      wifiStandard = WIFI_STANDARD_80211b;
      frequency = 2.4;
      channelWidth = 22;
    }
  else if (standard == "11g")
    {
      wifiStandard = WIFI_STANDARD_80211g;
      frequency = 2.4;
      channelWidth = 20;
    }
  else if (standard == "11n")
    {
      if (frequency == 2.4)
        {
          wifiStandard = WIFI_STANDARD_80211n_2_4GHZ;
        }
      else if (frequency == 5)
        {
          wifiStandard = WIFI_STANDARD_80211n_5GHZ;
        }
      else
        {
          NS_FATAL_ERROR ("Unsupported frequency band " << frequency << " MHz for standard " << standard);
        }
    }
  else if (standard == "11ac")
    {
      wifiStandard = WIFI_STANDARD_80211ac;
      frequency = 5;
    }
  else if (standard == "11ax")
    {
      if (frequency == 2.4)
        {
          wifiStandard = WIFI_STANDARD_80211ax_2_4GHZ;
        }
      else if (frequency == 5)
        {
          wifiStandard = WIFI_STANDARD_80211ax_5GHZ;
          apTxPower = 30.0;
          staTxPower = 24.0;
        }
      else if (frequency == 6)
        {
          wifiStandard = WIFI_STANDARD_80211ax_6GHZ;
          apTxPower = std::min(30.0, 10*log10(3.16 * channelWidth));
          staTxPower = std::min(24.0, 10*log10(0.8 * channelWidth));
          std::cout << apTxPower << std::endl;
        }
      else
        {
          NS_FATAL_ERROR ("Unsupported frequency band " << frequency << " MHz for standard " << standard);
        }
    }
  else
    {
      NS_FATAL_ERROR ("Unsupported standard: " << standard);
    }

  YansWifiPhyHelper wifiPhy;
  wifiPhy.DisablePreambleDetectionModel ();

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  if (frequency == 6)
    {
      // Reference Loss for Friss at 1 m with 6.0 GHz
      wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                                      "Exponent", DoubleValue (2.0),
                                      "ReferenceDistance", DoubleValue (1.0),
                                      "ReferenceLoss", DoubleValue (49.013));
    }
  else if (frequency == 5)
    {
      // Reference Loss for Friss at 1 m with 5.15 GHz
      wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                                      "Exponent", DoubleValue (2.0),
                                      "ReferenceDistance", DoubleValue (1.0),
                                      "ReferenceLoss", DoubleValue (46.6777));
    }
  else
    {
      // Reference Loss for Friss at 1 m with 2.4 GHz
      wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                                      "Exponent", DoubleValue (2.0),
                                      "ReferenceDistance", DoubleValue (1.0),
                                      "ReferenceLoss", DoubleValue (40.046));
    }

  WifiHelper wifi;
  wifi.SetStandard (wifiStandard);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));

  Gnuplot2dDataset dataset;
  Gnuplot2dDataset datasetBianchiEifs;
  Gnuplot2dDataset datasetBianchiDifs;
  dataset.SetErrorBars (Gnuplot2dDataset::Y);
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  datasetBianchiEifs.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  datasetBianchiDifs.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Experiment experiment;
  WifiMacHelper wifiMac;
  double averageThroughput, throughputArray[trials];
  for (uint32_t n = nMinStas; n <= nMaxStas; n += nStepSize)
    {
      averageThroughput = 0;
      double throughput;
      for (uint32_t runIndex = 0; runIndex < trials; runIndex++)
        {
          packetsReceived.clear ();
          bytesReceived.clear ();
          packetsTransmitted.clear ();
          psduFailed.clear ();
          psduSucceeded.clear ();
          phyHeaderFailed.clear ();
          timeFirstReceived.clear ();
          timeLastReceived.clear ();
          rxEventWhileDecodingPreamble.clear ();
          rxEventWhileRxing.clear ();
          rxEventWhileTxing.clear ();
          rxEventAbortedByTx.clear ();
          associated.clear ();
          throughput = 0;
          std::cout << "Trial " << runIndex + 1 << " of " << trials << "; "<< phyModeStr.str () << " for " << n << " nodes " << std::endl;
          if (tracing)
            {
              cwTraceFile << "# Trial " << runIndex + 1 << " of " << trials << "; "<< phyModeStr.str () << " for " << n << " nodes" << std::endl;
              backoffTraceFile << "# Trial " << runIndex + 1 << " of " << trials << "; "<< phyModeStr.str () << " for " << n << " nodes" << std::endl;
              phyTxTraceFile << "# Trial " << runIndex + 1 << " of " << trials << "; " << phyModeStr.str () << " for " << n << " nodes" << std::endl;
              macTxTraceFile << "# Trial " << runIndex + 1 << " of " << trials << "; " << phyModeStr.str () << " for " << n << " nodes" << std::endl;
              macRxTraceFile << "# Trial " << runIndex + 1 << " of " << trials << "; " << phyModeStr.str () << " for " << n << " nodes" << std::endl;
              socketSendTraceFile << "# Trial " << runIndex + 1 << " of " << trials << "; " << phyModeStr.str () << " for " << n << " nodes" << std::endl;
            }
          experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel, runIndex, n, duration, pcap, infra, channelWidth, guardIntervalNs, distance, apTxPower, staTxPower, pktInterval);
          uint32_t k = 0;
          if (bytesReceived.size () != n)
            {
              //NS_FATAL_ERROR ("Not all stations got traffic!");
            }
          for (auto it = bytesReceived.begin (); it != bytesReceived.end (); it++, k++)
            {
              Time first = timeFirstReceived.find (it->first)->second;
              Time last = timeLastReceived.find (it->first)->second;
              Time dataTransferDuration = last - first;
              double nodeThroughput = (it->second * 8 / static_cast<double> (dataTransferDuration.GetMicroSeconds ()));
              throughput += nodeThroughput;
              uint64_t nodeTxPackets = GetCount (packetsTransmitted, it->first);
              uint64_t nodeRxPackets = GetCount (packetsReceived, it->first);
              uint64_t nodePhyHeaderFailures = GetCount (phyHeaderFailed, it->first);
              uint64_t nodePsduFailures = GetCount (psduFailed, it->first);
              uint64_t nodePsduSuccess = GetCount (psduSucceeded, it->first);
              uint64_t nodeRxEventWhileDecodingPreamble = GetCount (rxEventWhileDecodingPreamble, it->first);
              uint64_t nodeRxEventWhileRxing = GetCount (rxEventWhileRxing, it->first);
              uint64_t nodeRxEventWhileTxing = GetCount (rxEventWhileTxing, it->first);
              uint64_t nodeRxEventAbortedByTx = GetCount (rxEventAbortedByTx, it->first);
              uint64_t nodeRxEvents =
                  nodePhyHeaderFailures + nodePsduFailures + nodePsduSuccess + nodeRxEventWhileDecodingPreamble +
                  nodeRxEventWhileRxing + nodeRxEventWhileTxing + nodeRxEventAbortedByTx;
              std::cout << "Node " << it->first
                        << ": TX packets " << nodeTxPackets
                        << "; RX packets " << nodeRxPackets
                        << "; PHY header failures " << nodePhyHeaderFailures
                        << "; PSDU failures " << nodePsduFailures
                        << "; PSDU success " << nodePsduSuccess
                        << "; RX events while decoding preamble " << nodeRxEventWhileDecodingPreamble
                        << "; RX events while RXing " << nodeRxEventWhileRxing
                        << "; RX events while TXing " << nodeRxEventWhileTxing
                        << "; RX events aborted by TX " << nodeRxEventAbortedByTx
                        << "; total RX events " << nodeRxEvents
                        << "; total events " << nodeTxPackets + nodeRxEvents
                        << "; time first RX " << first
                        << "; time last RX " << last
                        << "; dataTransferDuration " << dataTransferDuration
                        << "; throughput " << nodeThroughput  << " Mbps" << std::endl;
            }
          std::cout << "Total throughput: " << throughput << " Mbps" << std::endl;
          averageThroughput += throughput;
          throughputArray[runIndex] = throughput;
        }
      averageThroughput = averageThroughput / trials;

      bool rateFound = false;
      double relativeErrorDifs = 0;
      double relativeErrorEifs = 0;
      auto itDifs = bianchiResultsDifs.find (phyModeStr.str ());
      if (itDifs != bianchiResultsDifs.end ())
        {
          rateFound = true;
          auto it = itDifs->second.find (n);
          if (it != itDifs->second.end ())
            {
              relativeErrorDifs = (std::abs (averageThroughput - it->second) / it->second);
              std::cout << "Relative error (DIFS): " << 100 * relativeErrorDifs << "%" << std::endl;
            }
          else if (validate)
            {
              NS_FATAL_ERROR ("No Bianchi results (DIFS) calculated for that number of stations!");
            }
        }
      auto itEifs = bianchiResultsEifs.find (phyModeStr.str ());
      if (itEifs != bianchiResultsEifs.end ())
        {
          rateFound = true;
          auto it = itEifs->second.find (n);
          if (it != itEifs->second.end ())
            {
              relativeErrorEifs = (std::abs (averageThroughput - it->second) / it->second);
              std::cout << "Relative error (EIFS): " << 100 * relativeErrorEifs << "%" << std::endl;
            }
          else if (validate)
            {
              NS_FATAL_ERROR ("No Bianchi results (EIFS) calculated for that number of stations!");
            }
        }
      if (!rateFound && validate)
        {
          NS_FATAL_ERROR ("No Bianchi results calculated for that rate!");
        }
      double relativeError = std::min (relativeErrorDifs, relativeErrorEifs);
      if (validate && (relativeError > maxRelativeError))
        {
          NS_FATAL_ERROR ("Relative error is too high!");
        }

      double stDev = 0;
      for (uint32_t i = 0; i < trials; ++i)
        {
          stDev += pow (throughputArray[i] - averageThroughput, 2);
        }
      stDev = sqrt (stDev / (trials - 1));
      dataset.Add (n, averageThroughput, stDev);
    }
  dataset.SetTitle ("ns-3");

  auto itDifs = bianchiResultsDifs.find (phyModeStr.str ());
  if (itDifs != bianchiResultsDifs.end ())
    {
      for (uint32_t i = nMinStas; i <= nMaxStas; i += nStepSize)
        {
          double value = 0.0;
          auto it = itDifs->second.find (i);
          if (it != itDifs->second.end ())
            {
              value = it->second;
            }
          datasetBianchiDifs.Add (i, value);
        }
    }
  else
    {
      for (uint32_t i = nMinStas; i <= nMaxStas; i += nStepSize)
        {
          datasetBianchiDifs.Add (i, 0.0);
        }
    }

  auto itEifs = bianchiResultsEifs.find (phyModeStr.str ());
  if (itEifs != bianchiResultsEifs.end ())
    {
      for (uint32_t i = nMinStas; i <= nMaxStas; i += nStepSize)
        {
          double value = 0.0;
          auto it = itEifs->second.find (i);
          if (it != itEifs->second.end ())
            {
              value = it->second;
            }
          datasetBianchiEifs.Add (i, value);
        }
    }
  else
    {
      for (uint32_t i = nMinStas; i <= nMaxStas; i += nStepSize)
        {
          datasetBianchiEifs.Add (i, 0.0);
        }
    }

  datasetBianchiEifs.SetTitle ("Bianchi (EIFS - lower bound)");
  datasetBianchiDifs.SetTitle ("Bianchi (DIFS - upper bound)");
  gnuplot.AddDataset (dataset);
  gnuplot.SetTerminal ("postscript eps color enh \"Times-BoldItalic\"");
  gnuplot.SetLegend ("Number of competing stations", "Throughput (Mbps)");
  ss.str ("");
  ss << "Frame size " << pktSize << " bytes";
  gnuplot.SetTitle (ss.str ());
  ss.str ("");
  ss << "set xrange [" << nMinStas << ":" << nMaxStas << "]\n"
     << "set xtics " << nStepSize << "\n"
     << "set grid xtics ytics\n"
     << "set mytics\n"
     << "set style line 1 linewidth 5\n"
     << "set style line 2 linewidth 5\n"
     << "set style line 3 linewidth 5\n"
     << "set style line 4 linewidth 5\n"
     << "set style line 5 linewidth 5\n"
     << "set style line 6 linewidth 5\n"
     << "set style line 7 linewidth 5\n"
     << "set style line 8 linewidth 5\n"
     << "set style increment user";
  gnuplot.SetExtra (ss.str ());
  if (plotBianchiModel & 0x01)
    {
      datasetBianchiDifs.SetTitle ("Bianchi");
      gnuplot.AddDataset (datasetBianchiDifs);
    }
  if (plotBianchiModel & 0x02)
    {
      datasetBianchiEifs.SetTitle ("Bianchi");
      gnuplot.AddDataset (datasetBianchiEifs);
    }
  if (plotBianchiModel == 0x03)
    {
      datasetBianchiEifs.SetTitle ("Bianchi (EIFS - lower bound)");
      datasetBianchiDifs.SetTitle ("Bianchi (DIFS - upper bound)");
    }
  gnuplot.GenerateOutput (throughputPlot);
  throughputPlot.close ();


  if (tracing)
    {
      cwTraceFile.close ();
      backoffTraceFile.close ();
      phyTxTraceFile.close ();
      macTxTraceFile.close ();
      macRxTraceFile.close ();
      socketSendTraceFile.close ();
      snrTraceFile.close ();
    }
  
  std::map<double, uint64_t>::iterator iter; 
  for(iter = snrMap.begin() ; iter != snrMap.end() ; iter++ )
    {
       if (tracing)
        {
          snrTraceFile << "SNR is: " << iter->first << "; Total number is: " << iter->second << std::endl;
        }
       std::cout << "SNR is: " << iter->first << "; Total number is: " << iter->second << std::endl;
    }

  
  return 0;
}
