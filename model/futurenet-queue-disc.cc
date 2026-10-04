#include "futurenet-queue-disc.h"

#include "futurenet-traffic-tag.h"

#include "futurenet-deadline-tag.h"

#include "futurenet-edf-queue.h"

#include "ns3/drop-tail-queue.h"
#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FutureNetQueueDisc");
NS_OBJECT_ENSURE_REGISTERED(FutureNetQueueDisc);

FutureNetQueueDisc::FutureNetQueueDisc()
    : m_numPriorityClasses(4),
      m_defaultPriority(4),
      m_queueLimit(100),
      m_schedulingMode(0),
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
                          MakeUintegerChecker<uint32_t>(0, 2));

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

bool
FutureNetQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
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
        return GetInternalQueue(0)->Dequeue();
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

            // Find the packet with the earliest deadline.
            uint32_t selectedIndex = 0;
            Time earliestDeadline = Time::Max();

            for (uint32_t j = 0; j < items.size(); ++j)
            {
                DeadlineTag deadlineTag;
                Time deadline = Time::Max();

                if (items[j]->GetPacket()->PeekPacketTag(deadlineTag))
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
            for (uint32_t j = 0; j < items.size(); ++j)
            {
                if (j != selectedIndex)
                {
                    queue->Enqueue(items[j]);
                }
            }

            // Return the earliest-deadline packet.
            return items[selectedIndex];
        }

        return nullptr;
    }
    
    // Existing Strict Priority implementation
    for (uint32_t i = 0; i < m_numPriorityClasses; ++i)
    {
        if (!GetInternalQueue(i)->IsEmpty())
        {
            return GetInternalQueue(i)->Dequeue();
        }
    }

    return nullptr;
}


Ptr<const QueueDiscItem>
FutureNetQueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);

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
