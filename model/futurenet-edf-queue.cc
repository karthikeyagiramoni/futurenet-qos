#include "futurenet-edf-queue.h"

#include "futurenet-deadline-tag.h"
#include "ns3/nstime.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(FutureNetEdfQueue);

TypeId
FutureNetEdfQueue::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::FutureNetEdfQueue")
            .SetParent<Queue<QueueDiscItem>>()
            .SetGroupName("FutureNet")
            .AddConstructor<FutureNetEdfQueue>();

    return tid;
}

FutureNetEdfQueue::FutureNetEdfQueue()
{
}

bool
FutureNetEdfQueue::Enqueue(Ptr<QueueDiscItem> item)
{
    Time newDeadline = Time::Max();

    DeadlineTag newTag;

    if (item->GetPacket()->PeekPacketTag(newTag))
    {
        newDeadline = newTag.GetDeadline();
    }

    ConstIterator position = GetContainer().end();

    for (ConstIterator it = GetContainer().begin(); it != GetContainer().end(); ++it)
    {
        Time existingDeadline = Time::Max();

        DeadlineTag existingTag;

        if ((*it)->GetPacket()->PeekPacketTag(existingTag))
        {
            existingDeadline = existingTag.GetDeadline();
        }

        if (newDeadline < existingDeadline)
        {
            position = it;
            break;
        }
    }

    return DoEnqueue(position, item);
}

Ptr<QueueDiscItem>
FutureNetEdfQueue::Dequeue()
{
    if (GetContainer().empty())
    {
        return nullptr;
    }

    return DoDequeue(GetContainer().begin());
}

Ptr<QueueDiscItem>
FutureNetEdfQueue::Remove()
{
    if (GetContainer().empty())
    {
        return nullptr;
    }

    return DoRemove(GetContainer().begin());
}

Ptr<const QueueDiscItem>
FutureNetEdfQueue::Peek() const
{
    if (GetContainer().empty())
    {
        return nullptr;
    }

    return DoPeek(GetContainer().begin());
}

} // namespace ns3