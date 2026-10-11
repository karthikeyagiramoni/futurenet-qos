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
#include<vector>
#include <sstream>

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
                          MakeUintegerChecker<uint32_t>(2, 8))
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
    return true;
}

std::vector<uint32_t>
FutureNetQueueDisc::GetWrrWeights() const
{
    std::vector<uint32_t> weights;

    std::stringstream ss(m_wrrWeights);
    std::string value;

    while (std::getline(ss, value, ','))
    {
        uint32_t weight = std::stoul(value);

        if (weight == 0)
        {
            weight = 1;
        }

        weights.push_back(weight);
    }

    // Fill missing classes with weight 1.
    while (weights.size() < m_numPriorityClasses)
    {
        weights.push_back(1);
    }

    // Ignore extra weights.
    if (weights.size() > m_numPriorityClasses)
    {
        weights.resize(m_numPriorityClasses);
    }

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
FutureNetQueueDisc::DequeueEdf()
{
    Ptr<InternalQueue> selectedQueue = nullptr;
    Time earliestDeadline = Time::Max();


    for (uint32_t i = 0; i < m_numPriorityClasses; ++i)
    {
        Ptr<InternalQueue> queue = GetInternalQueue(i);

        if (queue->IsEmpty())
        {
            continue;
        }

        Ptr<const QueueDiscItem> item = queue->Peek();

        DeadlineTag deadlineTag;

        if (item->GetPacket()->PeekPacketTag(deadlineTag))
        {
            Time deadline = deadlineTag.GetDeadline();

            if (selectedQueue == nullptr || deadline < earliestDeadline)
            {
                selectedQueue = queue;
                earliestDeadline = deadline;
            }
        }
    }

    if (selectedQueue != nullptr)
    {
        return selectedQueue->Dequeue();
    }

    // No packet has a deadline.
    // Fall back to strict priority.
    for (uint32_t i = 0; i < m_numPriorityClasses; ++i)
    {
        if (!GetInternalQueue(i)->IsEmpty())
        {
            return GetInternalQueue(i)->Dequeue();
        }
    }

    return nullptr;
}

Ptr<QueueDiscItem>
FutureNetQueueDisc::CheckDeadline(Ptr<QueueDiscItem> item)
{
    if (item == nullptr)
    {
        return nullptr;
    }

    Time now = Simulator::Now();

    // A zero interval means check every dequeue.
    if (m_deadlineCheckInterval > Seconds(0) &&
        now - m_lastDeadlineCheck < m_deadlineCheckInterval)
    {
        return item;
    }

    m_lastDeadlineCheck = now;

    DeadlineTag deadlineTag;

    // No deadline means the packet cannot miss a deadline.
    if (!item->GetPacket()->PeekPacketTag(deadlineTag))
    {
        return item;
    }

    Time deadline = deadlineTag.GetDeadline();

    // Packet has not expired.
    if (now <= deadline)
    {
        return item;
    }

    // Deadline missed.
    m_deadlineMissCount++;

    FutureNetTrafficTag trafficTag;
    uint8_t priority = m_defaultPriority;
    uint32_t flowId = 0;

    if (item->GetPacket()->PeekPacketTag(trafficTag))
    {
        priority = trafficTag.GetPriority();
        flowId = trafficTag.GetFlowId();
    }

    Time lateness = now - deadline;

    if (m_enableDeadlineTracing)
    {
        m_deadlineMissTrace(flowId, priority, lateness);
    }

    // ExpiredPacketPolicy:
    // 0 = Drop
    // 1 = Transmit
    if (m_expiredPacketPolicy == 0)
    {
        if (m_enableDeadlineTracing)
        {
            m_expiredDropTrace(flowId, priority);
        }

        return nullptr;
    }

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
    m_enqueueTimes[item->GetPacket()->GetUid()] = Simulator::Now();
    if (m_schedulingMode == 1)
    {
        return GetInternalQueue(0)->Enqueue(item);
    }
    NS_LOG_FUNCTION(this << item);

    uint8_t priority = m_defaultPriority;

    FutureNetTrafficTag tag;

    if (item->GetPacket()->PeekPacketTag(tag))
    {
        priority = tag.GetPriority();
    }

    DeadlineTag deadlineTag;

    if (!item->GetPacket()->PeekPacketTag(deadlineTag) &&
        m_defaultDeadline != Time::Min())
    {
        deadlineTag.SetDeadline(m_defaultDeadline);
        item->GetPacket()->AddPacketTag(deadlineTag);
    }

    if (priority >= m_numPriorityClasses)
    {
        priority = m_numPriorityClasses - 1;
    }

    return GetInternalQueue(priority)->Enqueue(item);
}


Ptr<QueueDiscItem>
FutureNetQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    // Weighted Round Robin mode
    if (m_schedulingMode == 3)
    {
        std::vector<uint32_t> weights = GetWrrWeights();

        for (uint32_t attempts = 0; attempts < m_numPriorityClasses; ++attempts)
        {
            uint32_t priority = m_wrrCurrentClass;

            Ptr<InternalQueue> queue = GetInternalQueue(priority);

            if (!queue->IsEmpty())
            {
                Ptr<QueueDiscItem> item = queue->Dequeue();

                TraceClassDequeued(item);

                m_wrrPacketsServed++;

                // Move to the next class after its weight is exhausted.
                if (m_wrrPacketsServed >= weights[priority])
                {
                    m_wrrPacketsServed = 0;
                    m_wrrCurrentClass =
                        (m_wrrCurrentClass + 1) % m_numPriorityClasses;
                }

                return item;
            }

            // Current class is empty, skip to the next class.
            m_wrrPacketsServed = 0;
            m_wrrCurrentClass =
                (m_wrrCurrentClass + 1) % m_numPriorityClasses;
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
    // Peek must not modify the scheduler's state.
    if (m_schedulingMode == 3)
    {
        for (uint32_t attempts = 0;
            attempts < m_numPriorityClasses;
            ++attempts)
        {
            uint32_t priority =
                (m_wrrCurrentClass + attempts) % m_numPriorityClasses;

            if (!GetInternalQueue(priority)->IsEmpty())
            {
                return GetInternalQueue(priority)->Peek();
            }
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
