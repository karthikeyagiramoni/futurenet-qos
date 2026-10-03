#ifndef FUTURENET_QUEUE_DISC_H
#define FUTURENET_QUEUE_DISC_H

#include "ns3/queue-disc.h"

namespace ns3
{

class FutureNetQueueDisc : public QueueDisc
{
public:
    static TypeId GetTypeId();

    FutureNetQueueDisc();

    uint64_t GetDeadlineMissCount() const;

private:
    bool CheckConfig() override;
    void InitializeParams() override;

    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;

    Ptr<QueueDiscItem> DequeueEdf();

    uint32_t m_numPriorityClasses;
    uint8_t m_defaultPriority;
    uint32_t m_queueLimit;
    uint32_t m_schedulingMode;

    uint64_t m_deadlineMissCount;
};

} // namespace ns3

#endif // FUTURENET_QUEUE_DISC_H

