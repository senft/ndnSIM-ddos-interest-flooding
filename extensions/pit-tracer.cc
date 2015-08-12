#include "pit-tracer.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/config.h"
#include "ns3/names.h"
#include "ns3/callback.h"

// #include "ns3/ndn-app.h"
// #include "ns3/ndn-interest.h"
// #include "ns3/ndn-data.h"
#include "ns3/simulator.h"
#include "ns3/node-list.h"
#include "ns3/log.h"

// #include "ns3/ndnSIM/model/fw/nacks.h"

#include <boost/lexical_cast.hpp>

#include <fstream>

// #include "stats.h"

#include <ns3/ndnSIM/model/fw/best-route.h>
#include "strategies/satisfaction-based-interest-acceptance.h"
#include "strategies/satisfaction-based-pushback.h"

NS_LOG_COMPONENT_DEFINE ("ndn.PitTracer");

using namespace std;

namespace ns3 {
namespace ndn {

static std::list< boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<PitTracer> > > > g_tracers;

template<class T>
static inline void
NullDeleter (T *ptr)
{
}

void
PitTracer::Destroy ()
{
    g_tracers.clear ();
}

    void
PitTracer::InstallAll (const std::string &file, Time averagingPeriod/* = Seconds (0.5)*/)
{
    using namespace boost;
    using namespace std;

    std::list<Ptr<PitTracer> > tracers;
    boost::shared_ptr<std::ostream> outputStream;
    if (file != "-")
    {
        boost::shared_ptr<std::ofstream> os (new std::ofstream ());
        os->open (file.c_str (), std::ios_base::out | std::ios_base::trunc);

        if (!os->is_open ())
        {
            NS_LOG_ERROR ("File " << file << " cannot be opened for writing. Tracing disabled");
            return;
        }

        outputStream = os;
    }
    else
    {
        outputStream = boost::shared_ptr<std::ostream> (&std::cout, NullDeleter<std::ostream>);
    }

    for (NodeList::Iterator node = NodeList::Begin ();
            node != NodeList::End ();
            node++)
    {
        Ptr<PitTracer> trace = Install (*node, outputStream, averagingPeriod);
        tracers.push_back (trace);
    }

    if (tracers.size () > 0)
    {
        tracers.front ()->PrintHeader (*outputStream);
        *outputStream << "\n";
    }

    g_tracers.push_back (boost::make_tuple (outputStream, tracers));
}

    void
PitTracer::Install (const NodeContainer &nodes, const std::string &file, Time averagingPeriod/* = Seconds (0.5)*/)
{
    using namespace boost;
    using namespace std;

    std::list<Ptr<PitTracer> > tracers;
    boost::shared_ptr<std::ostream> outputStream;
    if (file != "-")
    {
        boost::shared_ptr<std::ofstream> os (new std::ofstream ());
        os->open (file.c_str (), std::ios_base::out | std::ios_base::trunc);

        if (!os->is_open ())
        {
            NS_LOG_ERROR ("File " << file << " cannot be opened for writing. Tracing disabled");
            return;
        }

        outputStream = os;
    }
    else
    {
        outputStream = boost::shared_ptr<std::ostream> (&std::cout, NullDeleter<std::ostream>);
    }

    for (NodeContainer::Iterator node = nodes.Begin ();
            node != nodes.End ();
            node++)
    {
        Ptr<PitTracer> trace = Install (*node, outputStream, averagingPeriod);
        tracers.push_back (trace);
    }

    if (tracers.size () > 0)
    {
        // *m_l3RateTrace << "# "; // not necessary for R's read.table
        tracers.front ()->PrintHeader (*outputStream);
        *outputStream << "\n";
    }

    g_tracers.push_back (boost::make_tuple (outputStream, tracers));
}

    void
PitTracer::Install (Ptr<Node> node, const std::string &file, Time averagingPeriod/* = Seconds (0.5)*/)
{
    using namespace boost;
    using namespace std;

    std::list<Ptr<PitTracer> > tracers;
    boost::shared_ptr<std::ostream> outputStream;
    if (file != "-")
    {
        boost::shared_ptr<std::ofstream> os (new std::ofstream ());
        os->open (file.c_str (), std::ios_base::out | std::ios_base::trunc);

        if (!os->is_open ())
        {
            NS_LOG_ERROR ("File " << file << " cannot be opened for writing. Tracing disabled");
            return;
        }

        outputStream = os;
    }
    else
    {
        outputStream = boost::shared_ptr<std::ostream> (&std::cout, NullDeleter<std::ostream>);
    }

    Ptr<PitTracer> trace = Install (node, outputStream, averagingPeriod);
    tracers.push_back (trace);

    if (tracers.size () > 0)
    {
        // *m_l3RateTrace << "# "; // not necessary for R's read.table
        tracers.front ()->PrintHeader (*outputStream);
        *outputStream << "\n";
    }

    g_tracers.push_back (boost::make_tuple (outputStream, tracers));
}


    Ptr<PitTracer>
PitTracer::Install (Ptr<Node> node,
        boost::shared_ptr<std::ostream> outputStream,
        Time averagingPeriod/* = Seconds (0.5)*/)
{
    NS_LOG_DEBUG ("Node: " << node->GetId ());

    Ptr<PitTracer> trace = Create<PitTracer> (outputStream, node);
    trace->SetAveragingPeriod (averagingPeriod);

    return trace;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

    PitTracer::PitTracer (boost::shared_ptr<std::ostream> os, Ptr<Node> node)
    : m_nodePtr (node)
      , m_os (os)
{
    m_node = boost::lexical_cast<string> (m_nodePtr->GetId ());

    Connect ();

    string name = Names::FindName (node);
    if (!name.empty ())
    {
        m_node = name;
    }
}

    PitTracer::PitTracer (boost::shared_ptr<std::ostream> os, const std::string &node)
    : m_node (node)
      , m_os (os)
{
    Connect ();
}

PitTracer::~PitTracer ()
{
};


    void
PitTracer::Connect ()
{
    Ptr<fw::SatisfactionBasedInterestAcceptance<fw::BestRoute> > acc = m_nodePtr->GetObject<fw::SatisfactionBasedInterestAcceptance<fw::BestRoute> > ();
    if(acc != 0)
        acc->TraceConnectWithoutContext("PITEntries", MakeCallback (&PitTracer::PitEntries, this));

    Ptr<fw::SatisfactionBasedInterestAcceptance<fw::BestRoute> > pb = m_nodePtr->GetObject<fw::SatisfactionBasedInterestAcceptance<fw::BestRoute> > ();
    if(pb != 0)
        pb->TraceConnectWithoutContext("PITEntries", MakeCallback (&PitTracer::PitEntries, this));

    Reset();
}


    void
PitTracer::SetAveragingPeriod (const Time &period)
{
    m_period = period;
    m_printEvent.Cancel ();
    m_printEvent = Simulator::Schedule (m_period, &PitTracer::PeriodicPrinter, this);
}

    void
PitTracer::PeriodicPrinter ()
{
    Print (*m_os);
    Reset ();

    m_printEvent = Simulator::Schedule (m_period, &PitTracer::PeriodicPrinter, this);
}

void
PitTracer::PrintHeader (std::ostream &os) const
{
    os << "Time" << "\t"
        << "Node" << "\t"
        << "Signal" << "\t"
        << "Value" << "\t";
}

    void
PitTracer::Reset ()
{
    entries = 0;
}

#define PRINTER(printName, fieldName)           \
  os << time.ToDouble (Time::S) << "\t"         \
  << m_node << "\t"                             \
  << printName << "\t"                          \
  << fieldName << "\n";

void
PitTracer::Print (std::ostream &os) const
{
    Time time = Simulator::Now ();
    PRINTER ("PitEntries", entries);
}

void PitTracer::PitEntries (uint32_t entries)
{
    this->entries = entries;
}

} // namespace ndn
} // namespace ns3
