/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 University of Washington
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
 * Author: Sébastien Deronne <sebastien.deronne@gmail.com>
 * Modified: Hao
 */

//
//  This example program can be used to experiment with spatial
//  reuse mechanisms of 802.11ax.
//
//  The geometry is as follows:
//
//                STA1          STA1
//                 |              |
//              d1 |              |d2
//                 |       d3     |
//                AP1 -----------AP2
//
//  STA1 and AP1 are in one BSS (with color set to 1), while STA2 and AP2 are in
//  another BSS (with color set to 2). The distances are configurable (d1 through d3).
//
//  STA1 is continously transmitting data to AP1, while STA2 is continuously sending data to AP2.
//  Each STA has configurable traffic loads (inter packet interval and packet size).
//  It is also possible to configure TX power per node as well as their CCA-ED tresholds.
//  OBSS_PD spatial reuse feature can be enabled (default) or disabled, and the OBSS_PD
//  threshold can be set as well (default: -72 dBm).
//  A simple Friis path loss model is used and a constant PHY rate is considered.
//
//  In general, the program can be configured at run-time by passing command-line arguments.
//  The following command will display all of the available run-time help options:
//    ./waf --run "wifi-spatial-reuse --help"
//
//  By default, the script shows the benefit of the OBSS_PD spatial reuse script:
//    ./waf --run wifi-spatial-reuse
//    Throughput for BSS 1: 6.6468 Mbit/s
//    Throughput for BSS 2: 6.6672 Mbit/s
//
// If one disables the OBSS_PD feature, a lower throughput is obtained per BSS:
//    ./waf --run "wifi-spatial-reuse --enableObssPd=0"
//    Throughput for BSS 1: 5.8692 Mbit/s
//    Throughput for BSS 2: 5.9364 Mbit/
//
// This difference between those results is because OBSS_PD spatial
// enables to ignore transmissions from another BSS when the received power
// is below the configured threshold, and therefore either defer during ongoing
// transmission or transmit at the same time.
//

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/application-container.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/wifi-net-device.h"
#include "ns3/ap-wifi-mac.h"
#include "ns3/he-configuration.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-server.h"

using namespace ns3;

uint32_t n = 1; // Number of STAs to scatter around each AP
uint32_t nBss = 2; // number of BSSs

std::vector<uint32_t> bytesReceived (4);
NodeContainer wifiApNodes;
NodeContainer wifiStaNodes;

uint32_t
ContextToNodeId (std::string context)
{
  std::string sub = context.substr (10);
  uint32_t pos = sub.find ("/Device");
  return atoi (sub.substr (0, pos).c_str ());
}

void
SocketRx (std::string context, Ptr<const Packet> p, const Address &addr)
{
  uint32_t nodeId = ContextToNodeId (context);
  bytesReceived[nodeId] += p->GetSize ();
  // std::cout << p->GetSize () << "  "<< Simulator::Now ().GetSeconds () <<std::endl;
}

int
mobilitySetup (MobilityHelper &mobility, const double d, const double r)
{
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  UniformDiscPositionAllocator unitDiscPosAllocator;

  bool fixedPosition = false;
  std::cout << "fixedPosition: " << fixedPosition << ", d=" << d << ", r=" << r << std::endl;

  // consistent stream to that positional layout is not perturbed by other configuration choices
  int64_t stream = 100;
  double theta, x, y;
  Vector v;

  // AP
  for (uint32_t bss = 0; bss < nBss; bss++)
    {
      theta = 2.0 * M_PI * (bss - 1) / 6.0;
      x = bss ? d * cos (theta) : 0.0;
      y = bss ? d * sin (theta) : 0.0;

      v = Vector (x, y, 0.0);
      positionAlloc->Add (v);
      std::cout << v << " m" << std::endl;
    }

  for (uint32_t bss = 0; bss < nBss; bss++)
    {
      // STA
      theta = 2.0 * M_PI * (bss - 1) / 6.0;
      x = bss ? d * cos (theta) : 0.0;
      y = bss ? d * sin (theta) : 0.0;
      // STAs for each AP are allocated uwing a different instance of a UnitDiscPositionAllocation.
      // To ensure unique randomness of positions, each allocator must be allocated a different stream number.
      unitDiscPosAllocator.AssignStreams (stream + bss);
      unitDiscPosAllocator.SetX (x);
      unitDiscPosAllocator.SetY (y);
      unitDiscPosAllocator.SetZ (0.0);
      unitDiscPosAllocator.SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          if (fixedPosition)
            v = Vector (x, y + r, 0.0);
          else
            v = unitDiscPosAllocator.GetNext ();
          positionAlloc->Add (v);
          std::cout << v << " m" << std::endl;
        }
    }

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (wifiApNodes);
  mobility.Install (wifiStaNodes);

  return 0;
}

int
main (int argc, char *argv[])
{
  double duration = 20.0; // seconds
  double d1 = 8; // meters
  double d2 = 8; // meters
  double d3 = 5.0; // meters
  double powSta1 = 20.0; // dBm
  double powSta2 = 20.0; // dBm
  double powAp1 = 20.0; // dBm
  double powAp2 = 20.0; // dBm
  double ccaEdTrSta1 = -82; // dBm
  double ccaEdTrSta2 = -82; // dBm
  double ccaEdTrAp1 = -82; // dBm
  double ccaEdTrAp2 = -82; // dBm
  uint32_t payloadSize = 1500; // bytes
  uint32_t mcs = 8; // MCS value
  double interval = 0.0001; // seconds
  bool enableObssPd = false;
  double obssPdThreshold = -72.0; // dBm

  int maxMpdus = 65535;
  CommandLine cmd (__FILE__);
  cmd.AddValue ("duration", "Duration of simulation (s)", duration);
  cmd.AddValue ("interval", "Inter packet interval (s)", interval);
  cmd.AddValue ("enableObssPd", "Enable/disable OBSS_PD", enableObssPd);
  cmd.AddValue ("n", "Number of STAs to scatter around each AP", n);
  cmd.AddValue ("d1", "Distance between STA1 and AP1 (m)", d1);
  cmd.AddValue ("d2", "Distance between STA2 and AP2 (m)", d2);
  cmd.AddValue ("d3", "Distance between AP1 and AP2 (m)", d3);
  cmd.AddValue ("powSta1", "Power of STA1 (dBm)", powSta1);
  cmd.AddValue ("powSta2", "Power of STA2 (dBm)", powSta2);
  cmd.AddValue ("powAp1", "Power of AP1 (dBm)", powAp1);
  cmd.AddValue ("powAp2", "Power of AP2 (dBm)", powAp2);
  cmd.AddValue ("ccaEdTrSta1", "CCA-ED Threshold of STA1 (dBm)", ccaEdTrSta1);
  cmd.AddValue ("ccaEdTrSta2", "CCA-ED Threshold of STA2 (dBm)", ccaEdTrSta2);
  cmd.AddValue ("ccaEdTrAp1", "CCA-ED Threshold of AP1 (dBm)", ccaEdTrAp1);
  cmd.AddValue ("ccaEdTrAp2", "CCA-ED Threshold of AP2 (dBm)", ccaEdTrAp2);
  cmd.AddValue ("mcs", "The constant MCS value to transmit HE PPDUs", mcs);
  cmd.Parse (argc, argv);
  Config::SetDefault ("ns3::QosTxop::UseExplicitBarAfterMissedBlockAck", BooleanValue (false));
  wifiApNodes.Create (nBss);
  wifiStaNodes.Create (nBss * n);
  bytesReceived.resize ((n + 1) * nBss);

  SpectrumWifiPhyHelper spectrumPhy;
  Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
  Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
     // more prominent example values:
  lossModel ->SetAttribute ("ReferenceDistance", DoubleValue (1)); 
  lossModel ->SetAttribute ("Exponent", DoubleValue (3.5)); 
  lossModel ->SetAttribute ("ReferenceLoss", DoubleValue (50)); 
  spectrumChannel->AddPropagationLossModel (lossModel);   
  Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
  spectrumChannel->SetPropagationDelayModel (delayModel);

  spectrumPhy.SetChannel (spectrumChannel);
  spectrumPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
  spectrumPhy.Set ("Frequency", UintegerValue (5180)); // channel 36 at 20 MHz
  spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel");
  spectrumPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  //TODO: add parameter to configure CCA-PD

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);
  if (enableObssPd)
    {
      wifi.SetObssPdAlgorithm ("ns3::ConstantObssPdAlgorithm",
                               "ObssPdLevel", DoubleValue (obssPdThreshold));
    }

  WifiMacHelper mac;
  std::ostringstream oss;
  oss << "VhtMcs" << mcs;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (oss.str ()),
                                "ControlMode", StringValue (oss.str ()));

  spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta1));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta1));
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaEdTrSta1));
  spectrumPhy.Set ("RxSensitivity", DoubleValue (-90.0));

  Ssid ssidA = Ssid ("A");
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssidA));
  NetDeviceContainer staDeviceA;
  for (uint32_t i = 0; i < n; i++)
    staDeviceA.Add (wifi.Install (spectrumPhy, mac, wifiStaNodes.Get (i)));

  spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp1));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp1));
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaEdTrAp1));
  spectrumPhy.Set ("RxSensitivity", DoubleValue (-90.0));

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssidA));
  NetDeviceContainer apDeviceA = wifi.Install (spectrumPhy, mac, wifiApNodes.Get (0));

  Ptr<WifiNetDevice> apDevice = apDeviceA.Get (0)->GetObject<WifiNetDevice> ();
  Ptr<ApWifiMac> apWifiMac = apDevice->GetMac ()->GetObject<ApWifiMac> ();
  if (enableObssPd)
    {
      apDevice->GetHeConfiguration ()->SetAttribute ("BssColor", UintegerValue (1));
    }

  spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta2));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta2));
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaEdTrSta2));
  spectrumPhy.Set ("RxSensitivity", DoubleValue (-90.0));
  spectrumPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  Ssid ssidB = Ssid ("B");
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssidB));
  NetDeviceContainer staDeviceB;
  for (uint32_t i = 0; i < n; i++)
    staDeviceB.Add (wifi.Install (spectrumPhy, mac, wifiStaNodes.Get (n + i)));

  spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp2));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp2));
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaEdTrAp2));
  spectrumPhy.Set ("RxSensitivity", DoubleValue (-90.0));
  spectrumPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssidB));
  NetDeviceContainer apDeviceB = wifi.Install (spectrumPhy, mac, wifiApNodes.Get (1));

  Ptr<WifiNetDevice> ap2Device = apDeviceB.Get (0)->GetObject<WifiNetDevice> ();
  apWifiMac = ap2Device->GetMac ()->GetObject<ApWifiMac> ();
  if (enableObssPd)
    {
      ap2Device->GetHeConfiguration ()->SetAttribute ("BssColor", UintegerValue (2));
    }

  // Assign positions to all nodes using position allocator
  MobilityHelper mobility;
  double r = d2;
  mobilitySetup (mobility, d3, r);

  PacketSocketHelper packetSocket;
  packetSocket.Install (wifiApNodes);
  packetSocket.Install (wifiStaNodes);
  ApplicationContainer apps;
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    for (uint32_t i = 0; i < wifiApNodes.GetN(); ++i)
    {
      Ptr<NetDevice> dev = wifiApNodes.Get (i)->GetDevice (0);
      Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
      wifi_dev->GetMac ()->SetAttribute ("BE_MaxAmpduSize", UintegerValue (maxMpdus * (payloadSize + 50)));
      wifi_dev->GetMac ()->SetAttribute ("BK_MaxAmpduSize", UintegerValue (maxMpdus * (payloadSize + 50)));
      wifi_dev->GetMac ()->SetAttribute ("VO_MaxAmpduSize", UintegerValue (maxMpdus * (payloadSize + 50)));
      wifi_dev->GetMac ()->SetAttribute ("VI_MaxAmpduSize", UintegerValue (maxMpdus * (payloadSize + 50)));
    }

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
      Ptr<NetDevice> dev = wifiStaNodes.Get (i)->GetDevice (0);
      Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
      wifi_dev->GetMac ()->SetAttribute ("BE_MaxAmpduSize", UintegerValue (maxMpdus * (payloadSize + 50)));
      wifi_dev->GetMac ()->SetAttribute ("BK_MaxAmpduSize", UintegerValue (maxMpdus * (payloadSize + 50)));
      wifi_dev->GetMac ()->SetAttribute ("VO_MaxAmpduSize", UintegerValue (maxMpdus * (payloadSize + 50)));
      wifi_dev->GetMac ()->SetAttribute ("VI_MaxAmpduSize", UintegerValue (maxMpdus * (payloadSize + 50)));
    }
  //BSS 1
  for (uint32_t i = 0; i < n; i++)
    {
      PacketSocketAddress socketAddr;
      socketAddr.SetSingleDevice (staDeviceA.Get (i)->GetIfIndex ());
      socketAddr.SetPhysicalAddress (apDeviceA.Get (0)->GetAddress ());
      socketAddr.SetProtocol (1);
      Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient> ();
      client->SetRemote (socketAddr);
      wifiStaNodes.Get (i)->AddApplication (client);
      client->SetAttribute ("PacketSize", UintegerValue (payloadSize));
      client->SetAttribute ("MaxPackets", UintegerValue (0));
      client->SetAttribute ("Interval", TimeValue (Seconds (interval)));
      Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer> ();
      server->SetLocal (socketAddr);
      wifiApNodes.Get (0)->AddApplication (server);
    }

  // BSS 2
  for (uint32_t i = 0; i < n; i++)
    {
      PacketSocketAddress socketAddr;
      socketAddr.SetSingleDevice (staDeviceB.Get (i)->GetIfIndex ());
      socketAddr.SetPhysicalAddress (apDeviceB.Get (0)->GetAddress ());
      socketAddr.SetProtocol (1);
      Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient> ();
      client->SetRemote (socketAddr);
      wifiStaNodes.Get (n + i)->AddApplication (client);
      client->SetAttribute ("PacketSize", UintegerValue (payloadSize));
      client->SetAttribute ("MaxPackets", UintegerValue (0));
      client->SetAttribute ("Interval", TimeValue (Seconds (interval)));
      Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer> ();
      server->SetLocal (socketAddr);
      wifiApNodes.Get (1)->AddApplication (server);
    }

  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::PacketSocketServer/Rx",
                   MakeCallback (&SocketRx));
  Config::SetDefault ("ns3::QosTxop::UseExplicitBarAfterMissedBlockAck", BooleanValue (false));
  Simulator::Stop (Seconds (duration));
  Simulator::Run ();

  Simulator::Destroy ();
  double total_tpt = 0;
  for (uint32_t i = 0; i < (n + 1) * nBss; i++)
    {
      double throughput = static_cast<double> (bytesReceived[i]) * 8 / 1000 / 1000 / duration / n;
      std::cout << "Throughput for node " << i + 1 << ": " << throughput << " Mbit/s" << std::endl;
      total_tpt += throughput;
    }
  std::cout << "Total Throughput: " << total_tpt << " Mbit/s" << std::endl;
  return 0;
}
