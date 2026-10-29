#include "ns3/core-module.h"
#include "ns3/network-module.h"

#include "ns3/futurenet-deadline-tag.h"
#include "ns3/futurenet-queue-disc.h"
#include "ns3/futurenet-traffic-tag.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace ns3;

namespace
{

constexpr uint32_t FLOW_COUNT = 4;

struct FlowConfig
{
    uint32_t flowId;
    uint8_t priority;
    uint32_t packetSizeBytes;
    uint32_t arrivalIntervalMs;
    uint32_t deadlineMs;
};

const std::array<FlowConfig, FLOW_COUNT> FLOW_CONFIGS = {{
    {1, 0, 100, 1, 10},
    {2, 1, 300, 2, 30},
    {3, 2, 700, 3, 60},
    {4, 3, 1200, 4, 100},
}};

struct FlowStats
{
    uint32_t generated{0};
    uint32_t enqueued{0};
    uint32_t enqueueDrops{0};
    uint32_t expiredDrops{0};
    uint32_t transmitted{0};
    uint32_t deadlineMisses{0};
    uint64_t transmittedBytes{0};
    uint64_t totalQueueDelayNs{0};
};

struct ExperimentConfig
{
    uint32_t durationMs{1000};
    uint32_t queueLimit{100};
    uint32_t serviceIntervalMs{2};
    uint8_t schedulingMode{0};
    uint8_t expiredPolicy{0};
    std::string csvFile{"futurenet-qos-advanced-results.csv"};
};

class AdvancedQueueDiscItem : public QueueDiscItem
{
  public:
    AdvancedQueueDiscItem(Ptr<Packet> packet,
                          const Address& address,
                          uint8_t priority,
                          uint32_t flowId)
        : QueueDiscItem(packet, address, 0)
    {
        FutureNetTrafficTag trafficTag;
        trafficTag.SetPriority(priority);
        trafficTag.SetFlowId(flowId);
        trafficTag.SetImportance(1.0 - static_cast<double>(priority) / (FLOW_COUNT - 1));
        packet->ReplacePacketTag(trafficTag);
    }

    void AddHeader() override
    {
    }

    bool Mark() override
    {
        return false;
    }
};

ExperimentConfig g_config;
Ptr<FutureNetQueueDisc> g_queueDisc;
std::array<FlowStats, FLOW_COUNT> g_flowStats;
std::unordered_map<uint64_t, Time> g_enqueueTimes;
uint64_t g_totalTransmittedBytes{0};
uint32_t g_totalTransmitted{0};

uint32_t
FindFlowIndex(uint32_t flowId)
{
    if (flowId == 0 || flowId > FLOW_COUNT)
    {
        NS_FATAL_ERROR("Unexpected flow ID in queue trace: " << flowId);
    }
    return flowId - 1;
}

void
RecordDeadlineMiss(uint32_t flowId, uint8_t, Time)
{
    ++g_flowStats[FindFlowIndex(flowId)].deadlineMisses;
}

void
RecordExpiredDrop(uint32_t flowId, uint8_t)
{
    ++g_flowStats[FindFlowIndex(flowId)].expiredDrops;
}

void
GeneratePacket(uint32_t flowIndex)
{
    const FlowConfig& flow = FLOW_CONFIGS.at(flowIndex);
    if (Simulator::Now() >= MilliSeconds(g_config.durationMs))
    {
        return;
    }

    FlowStats& stats = g_flowStats[flowIndex];
    ++stats.generated;

    Ptr<Packet> packet = Create<Packet>(flow.packetSizeBytes);
    DeadlineTag deadlineTag;
    deadlineTag.SetDeadline(Simulator::Now() + MilliSeconds(flow.deadlineMs));
    packet->AddPacketTag(deadlineTag);

    Ptr<QueueDiscItem> item =
        Create<AdvancedQueueDiscItem>(packet, Address(), flow.priority, flow.flowId);

    if (g_queueDisc->Enqueue(item))
    {
        ++stats.enqueued;
        g_enqueueTimes[packet->GetUid()] = Simulator::Now();
    }
    else
    {
        ++stats.enqueueDrops;
    }

    Simulator::Schedule(MilliSeconds(flow.arrivalIntervalMs), &GeneratePacket, flowIndex);
}

void
ServePacket()
{
    if (Simulator::Now() >= MilliSeconds(g_config.durationMs))
    {
        return;
    }

    Ptr<QueueDiscItem> item = g_queueDisc->Dequeue();
    if (item)
    {
        FutureNetTrafficTag trafficTag;
        if (!item->GetPacket()->PeekPacketTag(trafficTag))
        {
            NS_FATAL_ERROR("Dequeued packet is missing its FutureNet traffic tag");
        }

        FlowStats& stats = g_flowStats[FindFlowIndex(trafficTag.GetFlowId())];
        ++stats.transmitted;
        stats.transmittedBytes += item->GetPacket()->GetSize();
        g_totalTransmittedBytes += item->GetPacket()->GetSize();
        ++g_totalTransmitted;

        auto enqueueTime = g_enqueueTimes.find(item->GetPacket()->GetUid());
        if (enqueueTime == g_enqueueTimes.end())
        {
            NS_FATAL_ERROR("Missing enqueue timestamp for transmitted packet");
        }
        stats.totalQueueDelayNs +=
            static_cast<uint64_t>((Simulator::Now() - enqueueTime->second).GetNanoSeconds());
        g_enqueueTimes.erase(enqueueTime);
    }

    Simulator::Schedule(MilliSeconds(g_config.serviceIntervalMs), &ServePacket);
}

bool
WriteResults()
{
    uint32_t totalRemainingInQueue = 0;
    for (uint32_t i = 0; i < FLOW_COUNT; ++i)
    {
        const FlowStats& stats = g_flowStats[i];
        if (stats.enqueued < stats.expiredDrops + stats.transmitted)
        {
            NS_FATAL_ERROR("Per-flow packet counters exceed accepted packets");
        }

        const uint32_t remainingInQueue =
            stats.enqueued - stats.expiredDrops - stats.transmitted;
        const uint32_t accountedPackets =
            stats.enqueueDrops + stats.expiredDrops + stats.transmitted + remainingInQueue;
        if (stats.generated != accountedPackets)
        {
            NS_FATAL_ERROR("Per-flow packet accounting does not match generated packets");
        }
        totalRemainingInQueue += remainingInQueue;
    }

    if (totalRemainingInQueue != g_queueDisc->GetNPackets())
    {
        NS_FATAL_ERROR("Per-flow queue accounting does not match shared queue occupancy");
    }

    std::ofstream csv(g_config.csvFile, std::ios::trunc);
    if (!csv.is_open())
    {
        std::cerr << "Error: Could not open CSV file: " << g_config.csvFile << '\n';
        return false;
    }

    csv << "mode,flow_id,priority,packet_size_bytes,arrival_interval_ms,deadline_ms,"
           "generated,enqueued,enqueue_drops,expired_drops,transmitted,remaining_in_queue,"
           "deadline_misses,avg_queue_delay_ms,throughput_bps\n";

    const double durationSeconds = static_cast<double>(g_config.durationMs) / 1000.0;
    for (uint32_t i = 0; i < FLOW_COUNT; ++i)
    {
        const FlowConfig& flow = FLOW_CONFIGS[i];
        const FlowStats& stats = g_flowStats[i];
        const double averageDelayMs =
            stats.transmitted == 0
                ? 0.0
                : static_cast<double>(stats.totalQueueDelayNs) / stats.transmitted / 1e6;
        const double throughputBps =
            durationSeconds == 0.0
                ? 0.0
                : static_cast<double>(stats.transmittedBytes) * 8.0 / durationSeconds;
        const uint32_t remainingInQueue =
            stats.enqueued - stats.expiredDrops - stats.transmitted;

        csv << static_cast<uint32_t>(g_config.schedulingMode) << ','
            << flow.flowId << ','
            << static_cast<uint32_t>(flow.priority) << ','
            << flow.packetSizeBytes << ','
            << flow.arrivalIntervalMs << ','
            << flow.deadlineMs << ','
            << stats.generated << ','
            << stats.enqueued << ','
            << stats.enqueueDrops << ','
            << stats.expiredDrops << ','
            << stats.transmitted << ','
            << remainingInQueue << ','
            << stats.deadlineMisses << ','
            << std::fixed << std::setprecision(3) << averageDelayMs << ','
            << std::setprecision(2) << throughputBps << '\n';
    }

    return csv.good();
}

} // namespace

int
main(int argc, char* argv[])
{
    CommandLine command(__FILE__);
    command.AddValue("schedulingMode",
                     "0=Strict Priority, 1=EDF, 2=Hybrid, 3=WRR",
                     g_config.schedulingMode);
    command.AddValue("queueLimit", "Maximum number of packets in the shared queue", g_config.queueLimit);
    command.AddValue("serviceIntervalMs", "Interval between packet service opportunities",
                     g_config.serviceIntervalMs);
    command.AddValue("durationMs", "Simulation duration in milliseconds", g_config.durationMs);
    command.AddValue("expiredPolicy", "0=Drop expired packets, 1=Transmit expired packets",
                     g_config.expiredPolicy);
    command.AddValue("csvFile", "Per-flow CSV output filename", g_config.csvFile);
    command.Parse(argc, argv);

    if (g_config.schedulingMode > 3 || g_config.expiredPolicy > 1 || g_config.queueLimit == 0 ||
        g_config.serviceIntervalMs == 0 || g_config.durationMs == 0)
    {
        std::cerr << "Invalid experiment configuration.\n";
        return 1;
    }

    g_queueDisc = CreateObject<FutureNetQueueDisc>();
    g_queueDisc->SetAttribute("SchedulingMode", UintegerValue(g_config.schedulingMode));
    g_queueDisc->SetAttribute("NumPriorityClasses", UintegerValue(FLOW_COUNT));
    g_queueDisc->SetAttribute("QueueLimit", UintegerValue(g_config.queueLimit));
    g_queueDisc->SetAttribute("ExpiredPacketPolicy", UintegerValue(g_config.expiredPolicy));
    g_queueDisc->SetAttribute("WrrWeights", StringValue("4,3,2,1"));
    g_queueDisc->TraceConnectWithoutContext("DeadlineMiss", MakeCallback(&RecordDeadlineMiss));
    g_queueDisc->TraceConnectWithoutContext("ExpiredDrop", MakeCallback(&RecordExpiredDrop));
    g_queueDisc->Initialize();

    std::cout << "Starting advanced FutureNet QoS multi-flow experiment\n"
              << "Mode: " << static_cast<uint32_t>(g_config.schedulingMode) << '\n'
              << "Priority classes: 4 (0-3)\n"
              << "Queue limit: " << g_config.queueLimit << '\n'
              << "Service interval: " << g_config.serviceIntervalMs << " ms\n"
              << "Duration: " << g_config.durationMs << " ms\n";
    for (const FlowConfig& flow : FLOW_CONFIGS)
    {
        std::cout << "Flow " << flow.flowId << ": priority="
                  << static_cast<uint32_t>(flow.priority)
                  << ", packetSize=" << flow.packetSizeBytes << " bytes"
                  << ", arrivalInterval=" << flow.arrivalIntervalMs << " ms"
                  << ", deadline=" << flow.deadlineMs << " ms\n";
    }

    for (uint32_t i = 0; i < FLOW_COUNT; ++i)
    {
        Simulator::ScheduleNow(&GeneratePacket, i);
    }
    Simulator::ScheduleNow(&ServePacket);
    Simulator::Stop(MilliSeconds(g_config.durationMs));
    Simulator::Run();

    if (!WriteResults())
    {
        Simulator::Destroy();
        return 1;
    }

    std::cout << "\nPer-flow results\n"
              << "flow priority generated enqueued enqueue_drops expired_drops "
                 "transmitted remaining deadline_misses avg_delay_ms throughput_bps\n";
    for (uint32_t i = 0; i < FLOW_COUNT; ++i)
    {
        const FlowConfig& flow = FLOW_CONFIGS[i];
        const FlowStats& stats = g_flowStats[i];
        const double averageDelayMs =
            stats.transmitted == 0
                ? 0.0
                : static_cast<double>(stats.totalQueueDelayNs) / stats.transmitted / 1e6;
        const double throughputBps =
            static_cast<double>(stats.transmittedBytes) * 8.0 * 1000.0 / g_config.durationMs;
        const uint32_t remainingInQueue =
            stats.enqueued - stats.expiredDrops - stats.transmitted;

        std::cout << flow.flowId << ' ' << static_cast<uint32_t>(flow.priority) << ' '
                  << stats.generated << ' ' << stats.enqueued << ' ' << stats.enqueueDrops << ' '
                  << stats.expiredDrops << ' ' << stats.transmitted << ' ' << remainingInQueue << ' '
                  << stats.deadlineMisses << ' ' << std::fixed << std::setprecision(3)
                  << averageDelayMs << ' ' << std::setprecision(2) << throughputBps << '\n';
    }
    std::cout << "Total transmitted packets: " << g_totalTransmitted << '\n'
              << "Total throughput: "
              << static_cast<double>(g_totalTransmittedBytes) * 8.0 * 1000.0 /
                     g_config.durationMs
              << " bps\n"
              << "CSV file: " << g_config.csvFile << '\n';

    Simulator::Destroy();
    return 0;
}
