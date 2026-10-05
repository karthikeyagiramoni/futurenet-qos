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
#include<vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FutureNetQueueDisc");
NS_OBJECT_ENSURE_REGISTERED(FutureNetQueueDisc);

FutureNetQueueDisc::FutureNetQueueDisc()
    : m_numPriorityClasses(4),
      m_defaultPriority(4),
      m_queueLimit(100),
      m_schedulingMode(0),
      m_defaultDeadline(Time::Min()),
      m_expiredPacketPolicy(0),
      m_enableDeadlineTracing(true),
      m_deadlineMissCount(0)
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
                          "Queue scheduling mode: 0=StrictPriority, 1=EDF, 2=Hybrid.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&FutureNetQueueDisc::m_schedulingMode),
                          MakeUintegerChecker<uint32_t>(0, 2))
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

void
FutureNetQueueDisc::InitializeParams()
{
    if (m_schedulingMode == 1)
    {
        // EDF uses one independent deadline-ordered queue
        Ptr<FutureNetEdfQueue> queue = CreateObject<FutureNetEdfQueue>();

        queue->SetMaxSize(
            QueueSize(QueueSizeUnit::PACKETS, m_queueLimit));

        AddInternalQueue(queue);

        return;
    }

    // Existing strict-priority implementation
    for (uint32_t i = 0; i < m_numPriorityClasses; ++i)
    {
        Ptr<DropTailQueue<QueueDiscItem>> queue =
            CreateObject<DropTailQueue<QueueDiscItem>>();

        queue->SetMaxSize(
            QueueSize(QueueSizeUnit::PACKETS, m_queueLimit));

        AddInternalQueue(queue);
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

    DeadlineTag deadlineTag;

    // No deadline means the packet cannot miss a deadline.
    if (!item->GetPacket()->PeekPacketTag(deadlineTag))
    {
        return item;
    }

    Time deadline = deadlineTag.GetDeadline();
    Time now = Simulator::Now();

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
        // Find the highest-priority non-empty queue.
        for (uint32_t i = 0; i < m_numPriorityClasses; ++i)
        {
            Ptr<InternalQueue> queue = GetInternalQueue(i);

            if (queue->IsEmpty())
            {
                continue;
            }

            // Temporarily remove all packets from this priority queue.
            std::vector<Ptr<QueueDiscItem>> items;

            while (!queue->IsEmpty())
            {
            items.push_back(queue->Dequeue());
            }

            // Remove expired packets and keep valid packets.
            std::vector<Ptr<QueueDiscItem>> validItems;

            for (auto item : items)
            {
                Ptr<QueueDiscItem> checkedItem = CheckDeadline(item);

                if (checkedItem != nullptr)
                {
                validItems.push_back(checkedItem);
                }
            }

            // Put valid packets back if none remain to select.
            if (validItems.empty())
            {
                continue;
            }

            // Find the packet with the earliest deadline.
            uint32_t selectedIndex = 0;
            Time earliestDeadline = Time::Max();

            for (uint32_t j = 0; j < validItems.size(); ++j)
            {
                DeadlineTag deadlineTag;
                Time deadline = Time::Max();

                if (validItems[j]->GetPacket()->PeekPacketTag(deadlineTag))
                {
                    deadline = deadlineTag.GetDeadline();
                }

                if (deadline < earliestDeadline)
                {
                    earliestDeadline = deadline;
                    selectedIndex = j;
                }
            }

            // Put all non-selected packets back.
            for (uint32_t j = 0; j < validItems.size(); ++j)
            {
                if (j != selectedIndex)
                {
                    queue->Enqueue(validItems[j]);
                }
            }

            // Return the earliest-deadline packet.
            TraceClassDequeued(validItems[selectedIndex]);
            return validItems[selectedIndex];
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
