#ifndef FUTURENET_EDF_QUEUE_H
#define FUTURENET_EDF_QUEUE_H

#include "ns3/queue.h"
#include "ns3/queue-disc.h"

namespace ns3
{

class FutureNetEdfQueue : public Queue<QueueDiscItem>
{
public:
    static TypeId GetTypeId();

    FutureNetEdfQueue();

    bool Enqueue(Ptr<QueueDiscItem> item) override;

    Ptr<QueueDiscItem> Dequeue() override;

    Ptr<QueueDiscItem> Remove() override;

    Ptr<const QueueDiscItem> Peek() const override;
};

} // namespace ns3

#endif // FUTURENET_EDF_QUEUE_H
