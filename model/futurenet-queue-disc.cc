#include "futurenet-queue-disc.h"

#include "futurenet-traffic-tag.h"

#include "futurenet-deadline-tag.h"

#include "futurenet-edf-queue.h"

#include "ns3/drop-tail-queue.h"
#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include <vector>
#include <sstream>
#include <cstdint>
#include <limits>
#include <string>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FutureNetQueueDisc");
NS_OBJECT_ENSURE_REGISTERED(FutureNetQueueDisc);

FutureNetQueueDisc::FutureNetQueueDisc()
    : m_numPriorityClasses(4),
      m_defaultPriority(4),
      m_queueLimit(100),
      m_schedulingMode(0),
      m_wrrCurrentClass(0),
      m_wrrPacketsServed(0),
      m_defaultDeadline(Time::Min()),
      m_expiredPacketPolicy(0),
      m_enableDeadlineTracing(true),
      m_deadlineMissCount(0),
      m_deadlineCheckInterval(Seconds(0)),
      m_lastDeadlineCheck(Seconds(0))
{
}

TypeId
FutureNetQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::FutureNetQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("FutureNet")
            .AddConstructor<FutureNetQueueDisc>()
            .AddAttribute("NumPriorityClasses",
                          "Number of priority classes.",
                          UintegerValue(4),
                          MakeUintegerAccessor(
                              &FutureNetQueueDisc::m_numPriorityClasses),
                          MakeUintegerChecker<uint32_t>(1, 8))
            .AddAttribute("DefaultPriority",
                          "Default priority for packets without priority metadata.",
                          UintegerValue(4),
                          MakeUintegerAccessor(
                              &FutureNetQueueDisc::m_defaultPriority),
                          MakeUintegerChecker<uint8_t>(0, 7))
            .AddAttribute("QueueLimit",
                          "Maximum number of packets allowed in the queue.",
                          UintegerValue(100),
                          MakeUintegerAccessor(
                              &FutureNetQueueDisc::m_queueLimit),
                          MakeUintegerChecker<uint32_t>(1, 100000))
            .AddAttribute("SchedulingMode",
                            "Scheduling algorithm: 0=StrictPriority, 1=EDF, 2=Hybrid, 3=WeightedRoundRobin.",
                            UintegerValue(0),
                            MakeUintegerAccessor(&FutureNetQueueDisc::m_schedulingMode),
                            MakeUintegerChecker<uint32_t>(0, 3))
            .AddAttribute("WrrWeights",
                            "Weighted Round Robin weights for each priority class.",
                            StringValue("4,3,2,1"),
                            MakeStringAccessor(&FutureNetQueueDisc::m_wrrWeights),
                            MakeStringChecker())
            .AddAttribute("ExpiredPacketPolicy",
                          "Expired packet policy: 0=Drop, 1=Transmit.",
                          UintegerValue(0),
                          MakeUintegerAccessor(
                              &FutureNetQueueDisc::m_expiredPacketPolicy),
                          MakeUintegerChecker<uint32_t>(0, 1))
            .AddAttribute("DefaultDeadline",
                          "Default deadline assigned to packets without a DeadlineTag.",
                          TimeValue(Time::Min()),
                          MakeTimeAccessor(
                              &FutureNetQueueDisc::m_defaultDeadline),
                          MakeTimeChecker())
            .AddAttribute("DeadlineCheckInterval",
                          "Interval between deadline checks.",
                          TimeValue(Seconds(0)),
                          MakeTimeAccessor(&FutureNetQueueDisc::m_deadlineCheckInterval),
                          MakeTimeChecker())
            .AddAttribute("EnableDeadlineTracing",
                          "Enable deadline miss tracing.",
                          BooleanValue(true),
                          MakeBooleanAccessor(
                              &FutureNetQueueDisc::m_enableDeadlineTracing),
                          MakeBooleanChecker())
            .AddTraceSource("DeadlineMiss",
                            "Trace emitted when a packet misses its deadline.",
                            MakeTraceSourceAccessor(
                                &FutureNetQueueDisc::m_deadlineMissTrace),
                            "ns3::TracedCallback::Uint32Uint8Time")
            .AddTraceSource("ExpiredDrop",
                            "Trace emitted when an expired packet is dropped.",
                            MakeTraceSourceAccessor(
                                &FutureNetQueueDisc::m_expiredDropTrace),
                            "ns3::TracedCallback::Uint32Uint8")
            .AddTraceSource("ClassDequeued",
                            "Trace emitted when a packet is dequeued from a priority class.",
                            MakeTraceSourceAccessor(
                                &FutureNetQueueDisc::m_classDequeuedTrace),
                            "ns3::TracedCallback::Uint8Time");

    return tid;
}



bool
FutureNetQueueDisc::CheckConfig()
{
    NS_ABORT_MSG_IF(
        m_numPriorityClasses < 1 || m_numPriorityClasses > 8,
        "NumPriorityClasses must be between 1 and 8.");

    NS_ABORT_MSG_IF(
        m_schedulingMode > 3,
        "SchedulingMode must be 0, 1, 2, or 3.");

    NS_ABORT_MSG_IF(
        m_expiredPacketPolicy > 1,
        "ExpiredPacketPolicy must be 0 (Drop) or 1 (Transmit).");

    NS_ABORT_MSG_IF(
        m_deadlineCheckInterval < Seconds(0),
        "DeadlineCheckInterval cannot be negative.");

    // Validate WRR configuration only when WRR is selected.
    if (m_schedulingMode == 3)
    {
        const std::vector<uint32_t> weights = GetWrrWeights();
        NS_ABORT_MSG_IF(
            weights.size() != m_numPriorityClasses,
            "WRR weights do not match the number of priority classes.");
    }

    return true;
}

std::vector<uint32_t>
FutureNetQueueDisc::GetWrrWeights() const
{
    std::vector<uint32_t> weights;
    std::stringstream input(m_wrrWeights);
    std::string token;

    NS_ABORT_MSG_IF(m_wrrWeights.empty() || m_wrrWeights.back() == ',',
                "WrrWeights must not be empty or end with a comma");

    while (std::getline(input, token, ','))
    {
        std::stringstream valueStream(token);
        uint64_t weight = 0;

        valueStream >> std::ws;

        NS_ABORT_MSG_IF(token.empty() ||
                            !(valueStream >> weight),
                        "Invalid WRR weight: '" << token << "'");

        NS_ABORT_MSG_IF(token.empty() || token[0] == '-',
                "WRR weights must be positive integers: '" << token << "'");

        valueStream >> std::ws;

        NS_ABORT_MSG_IF(!valueStream.eof(),
                        "Unexpected characters in WRR weight: '" << token << "'");

        NS_ABORT_MSG_IF(weight == 0 ||
                            weight > std::numeric_limits<uint32_t>::max(),
                        "WRR weights must be in the range 1 to "
                            << std::numeric_limits<uint32_t>::max());

        weights.push_back(static_cast<uint32_t>(weight));
    }

    NS_ABORT_MSG_IF(weights.size() != m_numPriorityClasses,
                    "WrrWeights must contain exactly "
                        << m_numPriorityClasses
                        << " comma-separated values; got "
                        << weights.size());

    return weights;
}

void
FutureNetQueueDisc::InitializeParams()
{
    if (m_schedulingMode == 1)
    {
        auto queue = CreateObject<FutureNetEdfQueue>();
        queue->SetMaxSize(QueueSize(QueueSizeUnit::PACKETS, m_queueLimit));
        AddInternalQueue(queue);
        return;
    }

    for (uint32_t i = 0; i < m_numPriorityClasses; ++i)
    {
        if (m_schedulingMode == 2)
        {
            auto queue = CreateObject<FutureNetEdfQueue>();
            queue->SetMaxSize(QueueSize(QueueSizeUnit::PACKETS, m_queueLimit));
            AddInternalQueue(queue);
        }
        else
        {
            auto queue =
                CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>(
                    "MaxSize",
                    QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, m_queueLimit)));

            AddInternalQueue(queue);
        }
    }
}




Ptr<QueueDiscItem>
FutureNetQueueDisc::CheckDeadline(Ptr<QueueDiscItem> item)
{
    if (item == nullptr)
    {
        return nullptr;
    }

    // Always perform the final deadline check before transmission.
    const Time now = Simulator::Now();

    DeadlineTag deadlineTag;

    // Packets without a deadline do not miss one.
    if (!item->GetPacket()->PeekPacketTag(deadlineTag))
    {
        return item;
    }

    const Time deadline = deadlineTag.GetDeadline();

    // A packet is expired only when the current time is past its deadline.
    if (now <= deadline)
    {
        return item;
    }

    // Deadline missed.
    ++m_deadlineMissCount;

    FutureNetTrafficTag trafficTag;
    uint32_t flowId = 0;
    uint8_t priority = m_defaultPriority;

    if (item->GetPacket()->PeekPacketTag(trafficTag))
    {
        flowId = trafficTag.GetFlowId();
        priority = trafficTag.GetPriority();
    }

    const Time lateness = now - deadline;

    if (m_enableDeadlineTracing)
    {
        m_deadlineMissTrace(flowId, priority, lateness);
    }

    // ExpiredPacketPolicy: 0 = Drop, 1 = Transmit.
    if (m_expiredPacketPolicy == 0)
    {
        if (m_enableDeadlineTracing)
        {
            m_expiredDropTrace(flowId, priority);
        }

        // Remove the packet's enqueue-time entry to avoid a bookkeeping leak.
        m_enqueueTimes.erase(item->GetPacket()->GetUid());

        return nullptr;
    }

    // Transmit even though the deadline was missed.
    return item;
}

void
FutureNetQueueDisc::TraceClassDequeued(Ptr<QueueDiscItem> item)
{
    if (item == nullptr)
    {
        return;
    }

    uint8_t priority = m_defaultPriority;

    FutureNetTrafficTag trafficTag;
    if (item->GetPacket()->PeekPacketTag(trafficTag))
    {
        priority = trafficTag.GetPriority();
    }

    if (priority >= m_numPriorityClasses)
    {
        priority = m_numPriorityClasses - 1;
    }

    uint64_t packetUid = item->GetPacket()->GetUid();
    Time sojournTime = Seconds(0);

    auto it = m_enqueueTimes.find(packetUid);

    if (it != m_enqueueTimes.end())
    {
        sojournTime = Simulator::Now() - it->second;
        m_enqueueTimes.erase(it);
    }

    m_classDequeuedTrace(priority, sojournTime);
}


bool
FutureNetQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    if (item == nullptr)
    {
        return false;
    }

    // Count packets across all internal queues.
    // EDF has one queue; other modes have one queue per priority class.
    const uint32_t numQueues =
        (m_schedulingMode == 1) ? 1 : m_numPriorityClasses;

    uint32_t totalPackets = 0;

    for (uint32_t i = 0; i < numQueues; ++i)
    {
        totalPackets += GetInternalQueue(i)->GetNPackets();
    }

    // Enforce a global queue limit.
    if (totalPackets >= m_queueLimit)
    {
        DropBeforeEnqueue(item, "Global queue limit exceeded");
        return false;
    }

    // Apply the default deadline BEFORE branching into scheduling modes.
    DeadlineTag deadlineTag;

    if (!item->GetPacket()->PeekPacketTag(deadlineTag) &&
        m_defaultDeadline != Time::Min())
    {
        deadlineTag.SetDeadline(m_defaultDeadline);
        item->GetPacket()->AddPacketTag(deadlineTag);
    }

    // Select the destination queue.
    uint32_t queueIndex = 0;

    if (m_schedulingMode != 1)
    {
        uint8_t priority = m_defaultPriority;
        FutureNetTrafficTag trafficTag;

        if (item->GetPacket()->PeekPacketTag(trafficTag))
        {
            priority = trafficTag.GetPriority();
        }

        // Map out-of-range priorities to the lowest configured class.
        if (priority >= m_numPriorityClasses)
        {
            priority = m_numPriorityClasses - 1;
        }

        queueIndex = priority;
    }

    // Record enqueue time only after the queue accepts the packet.
    const uint64_t packetUid = item->GetPacket()->GetUid();

    const bool accepted = GetInternalQueue(queueIndex)->Enqueue(item);

    if (accepted)
    {
        m_enqueueTimes[packetUid] = Simulator::Now();
    }
    else
    {
        m_enqueueTimes.erase(packetUid);
    }

    return accepted;
}

Ptr<QueueDiscItem>
FutureNetQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    // Weighted Round Robin mode
    if (m_schedulingMode == 3)
    {
        const std::vector<uint32_t> weights = GetWrrWeights();
        uint32_t emptyClassesChecked = 0;

        while (emptyClassesChecked < m_numPriorityClasses)
        {
            const uint32_t priority = m_wrrCurrentClass;
            Ptr<InternalQueue> queue = GetInternalQueue(priority);

            if (queue->IsEmpty())
            {
                m_wrrPacketsServed = 0;
                m_wrrCurrentClass =
                    (m_wrrCurrentClass + 1) % m_numPriorityClasses;
                ++emptyClassesChecked;
                continue;
            }

            // A non-empty class was found.
            emptyClassesChecked = 0;

            Ptr<QueueDiscItem> item = queue->Dequeue();

            ++m_wrrPacketsServed;

            if (m_wrrPacketsServed >= weights[priority])
            {
                m_wrrPacketsServed = 0;
                m_wrrCurrentClass =
                    (m_wrrCurrentClass + 1) % m_numPriorityClasses;
            }

            item = CheckDeadline(item);

            if (item != nullptr)
            {
                TraceClassDequeued(item);
                return item;
            }

            // An expired packet was dropped. Continue searching.
        }

        return nullptr;
    }


    // EDF mode
    if (m_schedulingMode == 1)
    {
        while (!GetInternalQueue(0)->IsEmpty())
        {
            Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();

            item = CheckDeadline(item);

            if (item != nullptr)
            {
                TraceClassDequeued(item);
                return item;
            }
        }

        return nullptr;
    }

    // Hybrid mode
    if (m_schedulingMode == 2)
    {
        // Strict priority between classes,
        // EDF within the selected priority class.
        for (uint32_t i = 0; i < m_numPriorityClasses; ++i)
        {
            Ptr<InternalQueue> queue = GetInternalQueue(i);

            while (!queue->IsEmpty())
            {
                Ptr<QueueDiscItem> item = queue->Dequeue();

                item = CheckDeadline(item);

                if (item != nullptr)
                {
                    TraceClassDequeued(item);
                    return item;
                }
            }
        }

        return nullptr;
    }

    // Strict Priority implementation
    for (uint32_t i = 0; i < m_numPriorityClasses; ++i)
    {
        Ptr<InternalQueue> queue = GetInternalQueue(i);

        while (!queue->IsEmpty())
        {
            Ptr<QueueDiscItem> item = queue->Dequeue();

            item = CheckDeadline(item);

            if (item != nullptr)
            {
                TraceClassDequeued(item);
                return item;
            }
        }
    }


    return nullptr;
}


Ptr<const QueueDiscItem>
FutureNetQueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);

    // Weighted Round Robin mode.
    // Predict the next dequeue without modifying scheduler state.
    if (m_schedulingMode == 3)
    {
        const std::vector<uint32_t> weights = GetWrrWeights();

        uint32_t priority = m_wrrCurrentClass;
        uint32_t packetsServed = m_wrrPacketsServed;

        for (uint32_t attempts = 0;
             attempts < m_numPriorityClasses;
             ++attempts)
        {
            Ptr<InternalQueue> queue = GetInternalQueue(priority);

            if (queue->IsEmpty())
            {
                packetsServed = 0;
                priority = (priority + 1) % m_numPriorityClasses;
                continue;
            }

            // If this class has exhausted its turn, inspect the next class.
            if (packetsServed >= weights[priority])
            {
                packetsServed = 0;
                priority = (priority + 1) % m_numPriorityClasses;
                continue;
            }

            return queue->Peek();
        }

        return nullptr;
    }


    // EDF mode uses a single internal EDF queue.
    if (m_schedulingMode == 1)
    {
        if (!GetInternalQueue(0)->IsEmpty())
        {
            return GetInternalQueue(0)->Peek();
        }

        return nullptr;
    }

    // Strict Priority and Hybrid:
    // return the head packet from the highest-priority non-empty queue.
    for (uint32_t i = 0; i < m_numPriorityClasses; ++i)
    {
        if (!GetInternalQueue(i)->IsEmpty())
        {
            return GetInternalQueue(i)->Peek();
        }
    }

    return nullptr;
}

uint64_t
FutureNetQueueDisc::GetDeadlineMissCount() const
{
    return m_deadlineMissCount;
}

} // namespace ns3
