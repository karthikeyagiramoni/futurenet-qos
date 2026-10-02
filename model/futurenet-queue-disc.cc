#include "futurenet-queue-disc.h"

#include "ns3/drop-tail-queue.h"
#include "ns3/log.h"
#include "ns3/queue.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FutureNetQueueDisc");
NS_OBJECT_ENSURE_REGISTERED(FutureNetQueueDisc);

FutureNetQueueDisc::FutureNetQueueDisc()
    : m_deadlineMissCount(0)
{
}

TypeId
FutureNetQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::FutureNetQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("FutureNet")
            .AddConstructor<FutureNetQueueDisc>();

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
    Ptr<InternalQueue> queue = CreateObject<DropTailQueue<QueueDiscItem>>();

    AddInternalQueue(queue);
}

bool
FutureNetQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    // Temporary implementation:
    // Store packets in the first internal queue.
    GetInternalQueue(0)->Enqueue(item);

    return true;
}

Ptr<QueueDiscItem>
FutureNetQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    if (GetInternalQueue(0)->IsEmpty())
    {
        return nullptr;
    }

    return GetInternalQueue(0)->Dequeue();
}

Ptr<const QueueDiscItem>
FutureNetQueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);

    if (GetInternalQueue(0)->IsEmpty())
    {
        return nullptr;
    }

    return GetInternalQueue(0)->Peek();
}

uint64_t
FutureNetQueueDisc::GetDeadlineMissCount() const
{
    return m_deadlineMissCount;
}

} // namespace ns3
