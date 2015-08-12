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

#ifndef DDOS_APP_H_
#define DDOS_APP_H_

#include <ns3/core-module.h>
#include <ns3/ndnSIM-module.h>
// #include <ns3/ccnx-app.h>

using namespace ns3;
using namespace ndn;

class DdosApp : public App
{
public:
  static TypeId GetTypeId ();

  DdosApp ();
  virtual ~DdosApp ();

  virtual void
  OnNack (const Ptr<const Interest> &interest);

  virtual void
  OnData (const Ptr<const Data> &contentObject);

  /**
   * @brief Actually send packet
   */
  void
  SendPacket ();
  
protected:
  // from App
  virtual void
  StartApplication ();

  virtual void
  StopApplication ();


  void
  DelayedStop ();
  
private:
  UniformVariable m_rand; ///< @brief nonce generator
  RandomVariable *m_rand_time; ///< @brief nonce generator
  EventId         m_nextSendEvent;
  Time            m_lifetime;

  double m_frequency;
  // default interest
  // InterestHeader m_defaultInterest;
  Name m_prefix;
  bool m_evilBit;
  bool m_dataBasedLimit;

  // Zipf
  UniformVariable m_SeqRng;
  uint32_t m_N;  //number of the contents
  double m_q;  //q in (k+q)^s
  double m_s;  //s in (k+q)^s
  std::vector<double> m_Pcum;  //cumulative probability
  uint32_t GetNextSeq();
  void SetNumberOfContents (uint32_t numOfContents);

  // Evil
  UniformVariable     m_randomSeqId;
};

#endif
