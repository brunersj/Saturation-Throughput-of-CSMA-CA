// final code
/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * 
 * Authors: Charles Andre, Samuel Bruner
 * email: candre@usc.edu, sbruner@usc.edu
 * description: this code is used to test the performance of 802.11 binary exponential backoff
 * we compared these simulation results to Bianchi's paper and our own Analytical results
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mobility-helper.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include <ns3/txop.h>
#include <fstream>
#include <vector>
#include "ns3/gnuplot.h"

using namespace ns3;

double x [3][10];
double y [3][10];

NS_LOG_COMPONENT_DEFINE ("FinalScriptExample");

 //===========================================================================
 // Function: exp
 // Description: Runs an experiment to determine saturation throughput
 // given the size of the network, minCw, maxCw, plotnum, nodeindex
 //===========================================================================
int
exp (uint32_t nWifi, uint32_t MinCw, uint32_t MaxCw, uint32_t plotnum, uint32_t nodeindex)
{
  //////////////////////////////////////////////////
  // Initialize some variables
  //////////////////////////////////////////////////
  bool verbose = true;
  uint8_t nAP = 1;      // one access point
  double sim_time = 20.0;
  double start_time = 1.0;
  std::string dr = "6Mbps";
  std::string delay = "0.000001"; //1us of delay
  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  // hub
  NodeContainer wifiAPNode;
  wifiAPNode.Create (nAP);
  // spokes
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  
  //////////////////////////////////////////////////
  // Use Yans wifi channel helper
  //////////////////////////////////////////////////
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  //////////////////////////////////////////////////
  // Constant Rate Wifi
  //////////////////////////////////////////////////
  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  
  //////////////////////////////////////////////////
  // Configure the Hub
  //////////////////////////////////////////////////
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiAPNode);
  Ptr<NetDevice> dev = apDevices.Get(0);
  Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
  Ptr<WifiMac> wifi_mac = wifi_dev->GetMac ();
  PointerValue ptr;
  wifi_mac->GetAttribute ("Txop", ptr);
  Ptr<Txop> txop = ptr.Get<Txop> ();
  txop->SetMaxCw(MaxCw);
  txop->SetMinCw(MinCw);

  //////////////////////////////////////////////////
  // Configure the Spokes
  //////////////////////////////////////////////////
  Time slot_val = MicroSeconds(50);
  Time difs_val = MicroSeconds(128);
  Time sifs_val = MicroSeconds(28);
  Time ack_val = MicroSeconds(300);
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false),
               "Slot", (TimeValue) slot_val,
               "EifsNoDifs", (TimeValue)difs_val,
               "Sifs", (TimeValue)sifs_val,
               "AckTimeout", (TimeValue)ack_val );

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  //////////////////////////////////////////////////
  // Position Allocation
  //////////////////////////////////////////////////
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  double two_pi = 2*3.1415926535;
  int radius = 1;
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // for the AP
  
  for(int i = 0; i < int(nWifi); i++) 
    {
      double theta = two_pi * (i / nWifi);
      double x = radius * cos(theta);
      double y = radius * sin(theta);
      positionAlloc->Add(Vector(x,y,0.0));
    }

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  Ptr<Node> apWifiNode = wifiAPNode.Get (0);
  mobility.Install (apWifiNode);

  for(int i = 0; i < int(nWifi); i++) 
    {
      Ptr<Node> wifiStaNode = wifiStaNodes.Get (i);
      mobility.Install (wifiStaNode);
    }
  
  //////////////////////////////////////////////////
  // Install Internet Stack, Assign IP's
  //////////////////////////////////////////////////
  InternetStackHelper stack;
  stack.Install (wifiAPNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (apDevices);
  address.Assign (staDevices);

  //////////////////////////////////////////////////
  // Create a sink at the Hub
  //////////////////////////////////////////////////
  NS_LOG_INFO ("Create applications.");
  uint16_t port = 50000;
  Ptr<PacketSink> sink;               /* Pointer to the packet sink application */
  Address hubLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", hubLocalAddress);
  ApplicationContainer hubApp = packetSinkHelper.Install (wifiAPNode);
  sink = StaticCast<PacketSink> (hubApp.Get (0));
  hubApp.Start (Seconds (start_time));
  hubApp.Stop (Seconds (start_time + sim_time));

  //////////////////////////////////////////////////
  // Create OnOff applications to send UDP to the hub on each spoke node.
  //////////////////////////////////////////////////
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.1.3.1"), port));
  DataRate data_r("6Mbps");
  uint32_t packet_size = 2048;

  ApplicationContainer spokeApps;
  for (uint32_t i = 0; i < nWifi; ++i)
    {
      onOffHelper.SetAttribute ("DataRate", StringValue (dr));
      onOffHelper.SetConstantRate(data_r, packet_size);
      spokeApps.Add (onOffHelper.Install (wifiStaNodes.Get (i)));
    }
  spokeApps.Start (Seconds (start_time));
  spokeApps.Stop (Seconds (start_time + sim_time));

  //////////////////////////////////////////////////
  // Populate Routing Tables
  //////////////////////////////////////////////////
  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //////////////////////////////////////////////////
  // Run Experiment and Output Data
  //////////////////////////////////////////////////
  Simulator::Stop (Seconds (start_time + sim_time));
  Simulator::Run ();
  double NormAverageThroughput = ((sink->GetTotalRx () * 8) / (6 * 1e6 * (sim_time)));

  std::cout << nWifi << ", " << NormAverageThroughput << std::endl;

  x[plotnum][nodeindex] = nWifi;
  y[plotnum][nodeindex] = NormAverageThroughput;
  
  Simulator::Destroy ();
  return 0;
}

 //===========================================================================
 // Function: Create2DPlotFile
 // Description: This function creates a 2-D plot file.
 //===========================================================================
 void Create2DPlotFile ()
 {
   std::string fileNameWithNoExtension = "plot-2d";
   std::string graphicsFileName        = fileNameWithNoExtension + ".png";
   std::string plotFileName            = fileNameWithNoExtension + ".plt";
   std::string plotTitle               = "2-D Plot";
   std::string dataTitle               = "2-D Data";
 
   // Instantiate the plot and set its title.
   Gnuplot plot (graphicsFileName);
   plot.SetTitle (plotTitle);
 
   // Make the graphics file, which the plot file will create when it
   // is used with Gnuplot, be a PNG file.
   plot.SetTerminal ("png");
 
   // Set the labels for each axis.
   plot.SetLegend ("X Values", "Y Values");
 
   // Set the range for the x axis.
   plot.AppendExtra ("set xrange [0:50]");
 
   // Instantiate the dataset, set its title, and make the points be
   // plotted along with connecting lines.
   std::vector<Gnuplot2dDataset> dataset;
   dataset[0].SetTitle (dataTitle);
   dataset[0].SetStyle (Gnuplot2dDataset::LINES_POINTS);

   // Create the 2-D dataset.
    for (unsigned int i = 0; i < 3; i++)
      {
         for (unsigned int j = 0; j < 10; j++)
           {
             // Add this point.
             dataset[i].Add (x[i][j], y[i][j]);
           }
      }
 
   // Add the dataset to the plot.
   plot.AddDataset (dataset[0]);
   plot.AddDataset (dataset[1]);
   plot.AddDataset (dataset[2]);
 
   // Open the plot file.
   std::ofstream plotFile (plotFileName.c_str());
 
   // Write the plot file.
   plot.GenerateOutput (plotFile);
 
   // Close the plot file.
   plotFile.close ();
 }


int
main(int argc, char *argv[]) 
{
  uint32_t min_win[3] = {31,31,127};
  uint32_t max_win[3] = {255, 1023, 1023};
  int nodes[] = {5,10,15,20,25,30,50};
  for(int i = 0; i < 3; i++)
    {
      std::cout << min_win[i] << "," << max_win[i] << std::endl;
      int nodeindex = 0;
      for(auto num_nodes : nodes) 
        {
          exp(num_nodes, min_win[i], max_win[i],i,nodeindex);
          nodeindex++;
        }
      std::string fileName = std::to_string(min_win[i]) + std::to_string(max_win[i]) + ".dat";
      // Open the plot file.
      std::ofstream plotFile;
      plotFile.open(fileName.c_str());
      for (int j = 0; j < 7; j++)
        {
          plotFile << x[i][j] << " " << y[i][j] << "\n";
        }
      plotFile.close();

    }
  
  return 0;
}
