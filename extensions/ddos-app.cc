/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 PARC
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

#include "ddos-app.h"
#include <ns3/point-to-point-module.h>

NS_LOG_COMPONENT_DEFINE ("DdosApp");

NS_OBJECT_ENSURE_REGISTERED (DdosApp);

TypeId
DdosApp::GetTypeId ()
{
  static TypeId tid = TypeId ("DdosApp")
    .SetParent<App> ()
    .AddConstructor<DdosApp> ()

    .AddAttribute ("Frequency", "Frequency of interest packets",
                   StringValue ("1.0"),
                   MakeDoubleAccessor (&DdosApp::m_frequency),
                   MakeDoubleChecker<double> ())

    .AddAttribute ("Prefix","Name of the Interest",
                   StringValue ("/"),
                   MakeNameAccessor (&DdosApp::m_prefix),
                   MakeNameChecker ())
    .AddAttribute ("LifeTime","Interest lifetime",
                   StringValue ("1s"),
                   MakeTimeAccessor (&DdosApp::m_lifetime),
                   MakeTimeChecker ())
    .AddAttribute ("Evil", "Evil bit",
                   BooleanValue (false),
                   MakeBooleanAccessor (&DdosApp::m_evilBit),
                   MakeBooleanChecker ())
    .AddAttribute ("DataBasedLimits", "Calculate frequency based on how many data packets can be returned",
                   BooleanValue (true),
                   MakeBooleanAccessor (&DdosApp::m_dataBasedLimit),
                   MakeBooleanChecker ())
    ;
  return tid;
}

DdosApp::DdosApp ()
  : m_rand (0, std::numeric_limits<uint32_t>::max ())
  // , m_rand_time (0,1)
{
    m_randomSeqId = UniformVariable (1000 + 1, std::numeric_limits<uint32_t>::max ());
}

DdosApp::~DdosApp ()
{
}

void
DdosApp::OnNack (const Ptr<const Interest> &interest)
{
  // immediately send new packet, without taking into account periodicity
  // m_nextSendEvent.Cancel ();
  // m_nextSendEvent = Simulator::Schedule (Seconds (0.001),
  //       				 &DdosApp::SendPacket, this);
}

void
DdosApp::OnData (const Ptr<const Data> &data)
{
  // who cares
}

void
DdosApp::SendPacket ()
{
  int m_seq = GetNextSeq();
  // send packet
  Ptr<NameComponents> nameWithSequence = Create<NameComponents> (m_prefix);
  nameWithSequence->appendSeqNum (m_seq);

  Ptr<Interest> interest = Create<Interest> ();
  interest->SetNonce            (m_rand.GetValue ());
  interest->SetName             (nameWithSequence);
  interest->SetInterestLifetime (m_lifetime);
        
  NS_LOG_INFO ("> Interest for " << m_seq << ", lifetime " << m_lifetime.ToDouble (Time::S) << "s");

  m_face->ReceiveInterest (interest);
  m_transmittedInterests (interest, this, m_face);
  
  m_nextSendEvent = Simulator::Schedule (Seconds(m_rand_time->GetValue()), &DdosApp::SendPacket, this);
}

void
DdosApp::StartApplication ()
{
  // calculate outgoing rate and set Interest generation rate accordingly
  
  double sumOutRate = 0.0;
  
  Ptr<Node> node = GetNode ();
  for (uint32_t deviceId = 0; deviceId < node->GetNDevices (); deviceId ++)
    {
      Ptr<PointToPointNetDevice> device = DynamicCast<PointToPointNetDevice> (node->GetDevice (deviceId));
      if (device == 0)
        continue;

      DataRateValue dataRate; device->GetAttribute ("DataRate", dataRate);
      sumOutRate += (dataRate.Get ().GetBitRate () / 8);
    }

  m_rand_time = new UniformVariable (0.0, 2 * 1.0 / m_frequency);

  if (m_evilBit)
    {
      // std::cout << "evil freq: " << m_frequency << "s\n";
    }
  else
    {
      // std::cout << "good freq: " << m_frequency << "s\n";
    }
  
  App::StartApplication ();
  SetNumberOfContents(1000);
  SendPacket ();
}

void
DdosApp::StopApplication ()
{
  m_nextSendEvent.Cancel ();

  // std::cerr << "# references before delayed stop: " << m_face->GetReferenceCount () << std::endl;
  Simulator::Schedule (Seconds (10.0), &DdosApp::DelayedStop, this);
}

void
DdosApp::DelayedStop ()
{
  // std::cerr << "# references after delayed stop: " << m_face->GetReferenceCount () << std::endl;
  App::StopApplication ();
}

uint32_t
DdosApp::GetNextSeq()
{
    if(m_evilBit)
    {
        return m_randomSeqId.GetValue();
    }
    else
    {
        uint32_t content_index = 1; //[1, m_N]
        double p_sum = 0;

        double p_random = m_SeqRng.GetValue();
        while (p_random == 0)
        {
            p_random = m_SeqRng.GetValue();
        }
        // NS_LOG_LOGIC("p_random="<<p_random);
        for (uint32_t i=1; i<=m_N; i++)
        {
            p_sum = m_Pcum[i];   //m_Pcum[i] = m_Pcum[i-1] + p[i], p[0] = 0;   e.g.: p_cum[1] = p[1], p_cum[2] = p[1] + p[2]
            if (p_random <= p_sum)
            {
                content_index = i;
                break;
            }
        }
        // NS_LOG_DEBUG("RandomNumber="<<content_index);
        return content_index;
    }
}

void
DdosApp::SetNumberOfContents (uint32_t numOfContents)
{
    m_N = numOfContents - 1;

    // NS_LOG_DEBUG (m_q << " and " << m_s << " and " << m_N);

    m_Pcum = std::vector<double> (m_N + 1);

    m_Pcum[0] = 0.0;
    for (uint32_t i=1; i<=m_N; i++)
    {
        m_Pcum[i] = m_Pcum[i-1] + 1.0 / std::pow(i+m_q, m_s);
    }

    for (uint32_t i=1; i<=m_N; i++)
    {
        m_Pcum[i] = m_Pcum[i] / m_Pcum[m_N];
        // NS_LOG_LOGIC ("Cumulative probability [" << i << "]=" << m_Pcum[i]);
    }
}

