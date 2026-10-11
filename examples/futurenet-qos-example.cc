
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/futurenet-queue-disc.h"
#include "ns3/futurenet-traffic-tag.h"
#include "ns3/futurenet-deadline-tag.h"

#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FutureNetQosExample");

/**
 * A concrete QueueDiscItem for the example.
 */
class ExampleQueueDiscItem : public QueueDiscItem
{
public:
    ExampleQueueDiscItem(Ptr<Packet> packet,
                         const Address& address,
                         uint8_t priority,
                         uint32_t flowId,
                         double importance)
        : QueueDiscItem(packet, address, 0)
    {
        FutureNetTrafficTag trafficTag;
        trafficTag.SetPriority(priority);
        trafficTag.SetFlowId(flowId);
        trafficTag.SetImportance(importance);
        packet->ReplacePacketTag(trafficTag);
    }

    void AddHeader() override
    {
        // No network-layer header is required for this queue-only example.
    }

    bool Mark() override
    {
        return false;
    }
};

static void
PrintDequeue(uint8_t priority, Time time)
{
    std::cout << "  Dequeued priority class " << unsigned(priority)
              << " at " << time.GetMilliSeconds() << " ms" << std::endl;
}

static void
PrintDeadlineMiss(uint32_t flowId, uint8_t priority, Time lateness)
{
    std::cout << "  DEADLINE MISS: flow " << flowId
              << ", priority " << unsigned(priority)
              << ", lateness " << lateness.GetMilliSeconds()
              << " ms" << std::endl;
}

static void
PrintExpiredDrop(uint32_t flowId, uint8_t priority)
{
    std::cout << "  EXPIRED DROP: flow " << flowId
              << ", priority " << unsigned(priority)
              << std::endl;
}

int
main(int argc, char* argv[])
{
    uint32_t schedulingMode = 0;
    uint32_t queueLimit = 20;
    uint32_t expiredPolicy = 0;
    uint32_t dequeueTimeMs = 0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("schedulingMode",
                 "Scheduling mode: 0=Strict Priority, 1=EDF, "
                 "2=Hybrid, 3=WRR",
                 schedulingMode);
    cmd.AddValue("queueLimit", "Maximum total packets in the queue", queueLimit);
    cmd.AddValue("expiredPolicy",
                "Expired packet policy: 0=Drop, 1=Transmit",
                expiredPolicy);
    cmd.AddValue("dequeueTimeMs",
                "Simulation time in ms at which dequeuing begins",
                dequeueTimeMs);
    cmd.Parse(argc, argv);

    if (schedulingMode > 3)
    {
        std::cerr << "Invalid schedulingMode. Use 0, 1, 2, or 3."
                  << std::endl;
        return 1;
    }

    if (expiredPolicy > 1)
    {
        std::cerr << "Invalid expiredPolicy. Use 0 (Drop) or 1 (Transmit)."
                << std::endl;
        return 1;
    }

    Ptr<FutureNetQueueDisc> queueDisc = CreateObject<FutureNetQueueDisc>();

    queueDisc->SetAttribute("SchedulingMode", UintegerValue(schedulingMode));
    queueDisc->SetAttribute("QueueLimit", UintegerValue(queueLimit));
    queueDisc->SetAttribute("ExpiredPacketPolicy",
                            UintegerValue(expiredPolicy));

    queueDisc->TraceConnectWithoutContext(
        "DeadlineMiss",
        MakeCallback(&PrintDeadlineMiss));

    queueDisc->TraceConnectWithoutContext(
        "ExpiredDrop",
        MakeCallback(&PrintExpiredDrop));

    // Initialize the queue discipline and its internal queues.
    queueDisc->Initialize();

    // Print the priority class selected when a packet is dequeued.
    queueDisc->TraceConnectWithoutContext(
        "ClassDequeued",
        MakeCallback(&PrintDequeue));

    struct FlowInfo
    {
        uint32_t flowId;
        uint8_t priority;
        double importance;
        uint32_t deadlineMs;
    };

    // Five flows using three priority levels: 0 (highest), 1, and 2.
    const FlowInfo flows[] = {
        {1, 0, 0.95, 30},
        {2, 1, 0.75, 20},
        {3, 2, 0.50, 10},
        {4, 0, 0.90, 15},
        {5, 1, 0.65, 25},
    };

    std::cout << "FutureNet QoS example" << std::endl;
    std::cout << "Scheduling mode: " << schedulingMode << std::endl;
    std::cout << "Enqueuing five flows..." << std::endl;

    for (const auto& flow : flows)
    {
        Ptr<Packet> packet = Create<Packet>(100);

        // Attach traffic metadata.
        FutureNetTrafficTag trafficTag;
        trafficTag.SetPriority(flow.priority);
        trafficTag.SetFlowId(flow.flowId);
        trafficTag.SetImportance(flow.importance);
        packet->ReplacePacketTag(trafficTag);

        // Attach an absolute simulation-time deadline.
        DeadlineTag deadlineTag;
        deadlineTag.SetDeadline(MilliSeconds(flow.deadlineMs));
        packet->ReplacePacketTag(deadlineTag);

        Ptr<QueueDiscItem> item =
            Create<ExampleQueueDiscItem>(packet,
                                         Address(),
                                         flow.priority,
                                         flow.flowId,
                                         flow.importance);

        if (queueDisc->Enqueue(item))
        {
            std::cout << "  Enqueued flow " << flow.flowId
                      << ", priority " << unsigned(flow.priority)
                      << ", deadline " << flow.deadlineMs << " ms"
                      << std::endl;
        }
        else
        {
            std::cout << "  Queue full: flow " << flow.flowId
                      << " was rejected" << std::endl;
        }
    }

    Simulator::Schedule(MilliSeconds(dequeueTimeMs), [queueDisc]() {
        std::cout << "\nDequeuing packets at "
                << Simulator::Now().GetMilliSeconds()
                << " ms..." << std::endl;

        while (queueDisc->GetNPackets() > 0)
        {
            Ptr<QueueDiscItem> item = queueDisc->Dequeue();

            // An expired packet may be dropped by Dequeue().
            // Continue processing the remaining packets.
            if (item == nullptr)
            {
                continue;
            }

            FutureNetTrafficTag trafficTag;
            if (item->GetPacket()->PeekPacketTag(trafficTag))
            {
                std::cout << "  Flow " << trafficTag.GetFlowId()
                        << ", priority "
                        << unsigned(trafficTag.GetPriority())
                        << std::endl;
            }
        }
    });

    Simulator::Run();

    Simulator::Destroy();
    return 0;
}