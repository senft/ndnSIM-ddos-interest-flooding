/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 University of California, Los Angeles
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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"

#include <boost/lexical_cast.hpp>

using namespace ns3;
using namespace ns3::ndn;
using namespace std;

#include "calculate-max-capacity.h"

uint32_t Run = 1;

void PrintTime (Time next, const string name)
{
  cerr << " === " << name << " " << Simulator::Now ().ToDouble (Time::S) << "s" << endl;
  Simulator::Schedule (next, PrintTime, next, name);
}

int main (int argc, char**argv)
{
  string topology = "small-tree";
  string prefix = "";
  string producerLocation = "gw";
  Time evilGap = Time::FromDouble (0.02, Time::MS);
  Time defaultRtt = Seconds (0.25);
  uint32_t badCount = 1;
  uint32_t goodCount = 1;
  string folder = "tmp";
  
  CommandLine cmd;
  cmd.AddValue ("topology", "Topology", topology);
  cmd.AddValue ("run", "Run", Run);
  cmd.AddValue ("algorithm", "DDoS mitigation algorithm", prefix);
  cmd.AddValue ("producer", "Producer location: gw or bb", producerLocation);
  cmd.AddValue ("badCount", "Number of bad guys", badCount);
  cmd.AddValue ("goodCount", "Number of good guys", goodCount);
  cmd.AddValue ("folder", "Folder where results will be placed", folder);
  cmd.AddValue ("defaultRtt", "Default RTT for BDP limits", defaultRtt);
  cmd.Parse (argc, argv);

  Config::SetGlobal ("RngRun", IntegerValue (Run));
  StackHelper helper;
  helper.SetContentStore("ns3::ndn::cs::Nocache");
  helper.SetPit("ns3::ndn::pit::Persistent", "MaxSize", "5000");

  AppHelper evilAppHelper ("DdosApp");
  evilAppHelper.SetAttribute ("Evil", BooleanValue (true));
  evilAppHelper.SetAttribute ("LifeTime", StringValue ("2s"));
  evilAppHelper.SetAttribute ("DataBasedLimits", BooleanValue (false));
  
  AppHelper goodAppHelper ("DdosApp");
  goodAppHelper.SetAttribute ("LifeTime",  StringValue ("2s"));
  goodAppHelper.SetAttribute ("DataBasedLimits", BooleanValue (false));

  AppHelper ph ("ns3::ndn::Producer");
  ph.SetPrefix ("/good");
  ph.SetAttribute ("PayloadSize", StringValue("1100"));
  
  // string name = prefix;
  // name += "topo=" + topology;
  // name += "-evil-" + boost::lexical_cast<string> (badCount);
  // name += "-good-" + boost::lexical_cast<string> (goodCount);
  // name += "-producer-" + producerLocation;
  // name += "-run-"  + boost::lexical_cast<string> (Run);

  string name;
  name += "topo=" + topology;
  name += "_mar=0";
  name += "_ftbm=0";
  if (prefix == "satisfaction-accept")
    {
      name += "_detection=4";
    }
  else if (prefix == "satisfaction-pushback")
    {
      name += "_detection=5";
    }
  name += "_numServers=3";
  name += "_payloadSize=1100";
  name += "_numClients=42@100";
  name += "_numAttackers=14@1000";
  name += "_numMonitors=0";
  name += "_tau=0";
  name += "_observationPeriod=0s";
  name += "_gamma=0";
  name += "_cacheSize=0";
  name += "_pitSize=5000";
  name += "_pitLifetime=2s";
  name += "_run=1";
  name += "_seed=" + boost::lexical_cast<string>(Run);
  
  string results_file = "results/" + folder + "/" + name + "-l3trace.txt";
  string meta_file    = "results/" + folder + "/" + name + ".meta";
  string graph_dot_file    = "results/" + folder + "/" + name + ".dot";
  string graph_pdf_file    = "results/" + folder + "/" + name + ".pdf";
  
  if (prefix == "simple-limits")
    {
      helper.EnableLimits (true, defaultRtt);
      helper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::PerOutFaceLimits",
                                    "Limit", "ns3::ndn::Limits::Window");
    }
  else if (prefix == "fairness")
    {
      helper.EnableLimits (true, defaultRtt);  
      helper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::TokenBucketWithPerInterfaceFairness",
                                    "Limit", "ns3::ndn::Limits::Window");
    }
  else if (prefix == "satisfaction-accept")
    {
      helper.EnableLimits (true, defaultRtt);  
      helper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::Stats::SatisfactionBasedInterestAcceptance::PerOutFaceLimits",
                                    "GraceThreshold", "0.05");
    }
  else if (prefix == "satisfaction-pushback")
    {
      helper.EnableLimits (true, defaultRtt);  
      helper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::Stats::SatisfactionBasedPushback::TokenBucketWithPerInterfaceFairness",
                                    "GraceThreshold", "0.01");
    }
  else
    {
      cerr << "Invalid scenario prefix" << endl;
      return -1;
    }

  AnnotatedTopologyReader topologyReader ("", 1.0);
  topologyReader.SetFileName ("topologies/" + topology + ".txt");
  topologyReader.Read ();
  
  helper.Install (topologyReader.GetNodes ());

  topologyReader.ApplyOspfMetric ();
  
  GlobalRoutingHelper grouter;
  grouter.Install (topologyReader.GetNodes ());
  
  NodeContainer leaves;
  NodeContainer gw;
  NodeContainer bb;
  for_each (NodeList::Begin (), NodeList::End (), [&] (Ptr<Node> node) {
      if (Names::FindName (node).compare (0, 5, "leaf-")==0)
        {
          leaves.Add (node);
        }
      else if (Names::FindName (node).compare (0, 3, "gw-")==0)
        {
          gw.Add (node);
        }
      else if (Names::FindName (node).compare (0, 3, "bb-")==0)
        {
          bb.Add (node);
        }
    });

  system (("mkdir -p \"results/" + folder + "\"").c_str ());
  ofstream os (meta_file.c_str(), ios_base::out | ios_base::trunc);
  
  os << "Total_numbef_of_nodes      " << NodeList::GetNNodes () << endl;
  os << "Total_number_of_leaf_nodes " << leaves.GetN () << endl;
  os << "Total_number_of_gw_nodes   " << gw.GetN () << endl;
  os << "Total_number_of_bb_nodes   " << bb.GetN () << endl;

  NodeContainer producerNodes;
  
  NodeContainer evilNodes;
  NodeContainer goodNodes;

  set< Ptr<Node> > producers;
  set< Ptr<Node> > evils;
  set< Ptr<Node> > angels;

  if (goodCount == 0)
    {
      goodCount = leaves.GetN () - badCount;
    }

  if (goodCount < 1)
    {
      NS_FATAL_ERROR ("Number of good guys should be at least 1");
      exit (1);
    }
  
  if (leaves.GetN () < goodCount+badCount)
    {
      NS_FATAL_ERROR ("Number of good and bad guys ("<< (goodCount+badCount) <<") cannot be less than number of leaves in the topology ("<< leaves.GetN () <<")");
      exit (1);
    }

  if (producerLocation == "gw")
    {
      if (gw.GetN () < 1)
        {
          NS_FATAL_ERROR ("Topology does not have gateway nodes that can serve as producers");
          exit (1);
        }
    }
  else if (producerLocation == "bb")
    {
      if (bb.GetN () < 1)
        {
          NS_FATAL_ERROR ("Topology does not have backbone nodes that can serve as producers");
          exit (1);
        }
    }
  else
    {
      NS_FATAL_ERROR ("--producer can be either 'gw' or 'bb'");
      exit (1);
    }
  
  os << "Number of evil nodes: " << badCount << endl;
  while (evils.size () < badCount)
    {
      UniformVariable randVar (0, leaves.GetN ());
      Ptr<Node> node = leaves.Get (randVar.GetValue ());

      if (evils.find (node) != evils.end ())
        continue;
      evils.insert (node);
      
      string name = Names::FindName (node);
      Names::Rename (name, "evil-"+name);
    }

  while (angels.size () < goodCount)
    {
      UniformVariable randVar (0, leaves.GetN ());
      Ptr<Node> node = leaves.Get (randVar.GetValue ());
      if (angels.find (node) != angels.end () ||
          evils.find (node) != evils.end ())
        continue;
      
      angels.insert (node);
      string name = Names::FindName (node);
      Names::Rename (name, "good-"+name);
    }

  for_each (gw.Begin (), gw.End (), [&] (Ptr<Node> node) {
      producers.insert (node);
      string name = Names::FindName (node);
      Names::Rename (name, "producer-" + name);
    });
  
  auto assignNodes = [&os](NodeContainer &aset, const string &str) {
    return [&os, &aset, &str] (Ptr<Node> node)
    {
      string name = Names::FindName (node);
      os << name << " ";
      aset.Add (node);
    };
  };
  os << endl;
  
  // a little bit of C++11 flavor, compile with -std=c++11 flag
  os << "Evil: ";
  std::for_each (evils.begin (), evils.end (), assignNodes (evilNodes, "Evil"));
  os << "\nGood: ";
  std::for_each (angels.begin (), angels.end (), assignNodes (goodNodes, "Good"));
  os << "\nProducers: ";
  std::for_each (producers.begin (), producers.end (), assignNodes (producerNodes, "Producers"));
  os << "\n";
  
  grouter.AddOrigins ("/", producerNodes);
  grouter.CalculateRoutes ();

  // verify topology RTT
  for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node ++)
    {
      Ptr<Fib> fib = (*node)->GetObject<Fib> ();
      if (fib == 0 || fib->Begin () == 0) continue;

      if (2* fib->Begin ()->m_faces.begin ()->GetRealDelay ().ToDouble (Time::S) > defaultRtt.ToDouble (Time::S))
        {
          cout << "DefaultRTT is smaller that real RTT in the topology: " << 2*fib->Begin ()->m_faces.begin ()->GetRealDelay ().ToDouble (Time::S) << "s" << endl;
          os << "DefaultRTT is smaller that real RTT in the topology: " << 2*fib->Begin ()->m_faces.begin ()->GetRealDelay ().ToDouble (Time::S) << "s" << endl;
        }
    }

  double maxNonCongestionShare = 0.8 * calculateNonCongestionFlows (goodNodes, producerNodes);
  os << "maxNonCongestionShare   " << maxNonCongestionShare << endl;

  // saveActualGraph (graph_dot_file, NodeContainer (goodNodes, evilNodes));
  // system (("twopi -Tpdf \"" + graph_dot_file + "\" > \"" + graph_pdf_file + "\"").c_str ());
  cout << "Write effective topology graph to: " << graph_pdf_file << endl;
  cout << "Max non-congestion share:   " << maxNonCongestionShare << endl;

  // exit (1);
    
  for (NodeContainer::Iterator node = goodNodes.Begin (); node != goodNodes.End (); node++)
    {
      ApplicationContainer goodApp;
      goodAppHelper.SetPrefix ("/good/"+Names::FindName (*node));
      goodAppHelper.SetAttribute ("Frequency", DoubleValue(100));
      
      goodApp.Add (goodAppHelper.Install (*node));

      goodApp.Start (Seconds (0.0));
    }
  
  for (NodeContainer::Iterator node = evilNodes.Begin (); node != evilNodes.End (); node++)
    {
      ApplicationContainer evilApp;
      evilAppHelper.SetPrefix ("/evil/"+Names::FindName (*node));
      evilAppHelper.SetAttribute ("Frequency", DoubleValue(1000));
      evilApp.Add (evilAppHelper.Install (*node));
      
      evilApp.Start (Minutes (1));
      evilApp.Stop (Minutes (6));
    }
  
  ph.Install (producerNodes);

  L3AggregateTracer::InstallAll (results_file, Seconds (10.0));
  
  Simulator::Schedule (Seconds (10.0), PrintTime, Seconds (10.0), name);

  Simulator::Stop (Minutes (9.0));
  Simulator::Run ();
  Simulator::Destroy ();
 
  L3RateTracer::Destroy ();

  // cerr << "Archiving to: " << results_file << ".bz2" << endl;
  // system (("rm -f \"" + results_file + ".bz2" + "\"").c_str() );
  // system (("bzip2 \"" + results_file + "\"").c_str() );
  
  return 0;
}
