#include "ns3/core-module.h"
#include "ns3/network-module.h"

#include "ns3/futurenet-queue-disc.h"
#include "ns3/futurenet-traffic-tag.h"
#include "ns3/futurenet-deadline-tag.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace ns3;

// ------------------------------------------------------------
// QueueDiscItem used by the example
// ------------------------------------------------------------
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
        // No network header is needed for this queue-level example.
    }

    bool Mark() override
    {
        return false;
    }
};

// ------------------------------------------------------------
// Experiment configuration
// ------------------------------------------------------------
struct ExperimentConfig
{
    uint32_t packetCount = 100;
    uint32_t queueLimit = 20;
    uint32_t maxPriority = 4;
    uint32_t noDeadlinePercent = 0;
    std::string deadlineMode = "high";
    uint32_t customMinDeadlineMs = 1;
    uint32_t customMaxDeadlineMs = 60000;
    uint32_t arrivalIntervalMs = 1;
    uint32_t serviceIntervalMs = 2;
    uint32_t durationMs = 250;

    uint8_t schedulingMode = 0;
    uint8_t expiredPolicy = 0;
    bool fairnessTest = false;

    std::string csvFile = "futurenet-qos-results.csv";
};

static ExperimentConfig g_config;
static Ptr<FutureNetQueueDisc> g_queueDisc;

// ------------------------------------------------------------
// Metrics
// ------------------------------------------------------------
static uint32_t g_enqueued = 0;
static uint32_t g_transmitted = 0;
static uint32_t g_enqueueDrops = 0;
static uint32_t g_expiredDrops = 0;

static uint64_t g_transmittedBytes = 0;
static uint32_t g_transmittedByPriority[8] = {0};
static uint64_t g_totalQueueDelayNs = 0;
static uint32_t g_queueDelaySamples = 0;
static uint32_t g_generated = 0;
static uint32_t g_generatedWithoutDeadline = 0;

// ------------------------------------------------------------
// Deadline-miss callback
// Existing trace signature: (flowId, priority, deadline)
// ------------------------------------------------------------
static void
PrintDeadlineMiss(uint32_t flowId, uint8_t priority, Time deadline)
{
    std::cout << "[DeadlineMiss]"
              << " flow=" << flowId
              << " priority=" << static_cast<uint32_t>(priority)
              << " deadline=" << deadline.GetMilliSeconds()
              << " ms"
              << " now=" << Simulator::Now().GetMilliSeconds()
              << " ms\n";
}

// ------------------------------------------------------------
// Expired-drop callback
// Existing trace signature: (flowId, priority)
// ------------------------------------------------------------
static void
RecordExpiredDrop(uint32_t flowId, uint8_t priority)
{
    ++g_expiredDrops;

    std::cout << "[ExpiredDrop]"
              << " flow=" << flowId
              << " priority=" << static_cast<uint32_t>(priority)
              << " time=" << Simulator::Now().GetMilliSeconds()
              << " ms\n";
}

// ------------------------------------------------------------
// ClassDequeued callback
// Existing trace signature: (priority, sojournTime)
// ------------------------------------------------------------
static void
RecordDequeued(uint8_t priority, Time sojournTime)
{
    ++g_queueDelaySamples;
    g_totalQueueDelayNs +=
        static_cast<uint64_t>(sojournTime.GetNanoSeconds());

    if (priority < 8)
    {
        ++g_transmittedByPriority[priority];
    }

    std::cout << "[Dequeued]"
              << " priority=" << static_cast<uint32_t>(priority)
              << " queueDelay=" << sojournTime.GetMilliSeconds()
              << " ms\n";
}

// ------------------------------------------------------------
// Generate packets periodically
// Deadlines are relative to each packet's arrival time.
// ------------------------------------------------------------
static void
GeneratePacket(uint32_t sequence)
{
    if (sequence >= g_config.packetCount ||
        Simulator::Now() >= MilliSeconds(g_config.durationMs))
    {
        return;
    }

    const uint32_t flowId = 1 + (sequence % 5);
    const uint8_t priority =
        static_cast<uint8_t>(sequence % (g_config.maxPriority + 1));

    const double importance =
        g_config.maxPriority == 0
            ? 1.0
            : 1.0 - static_cast<double>(priority) / g_config.maxPriority;

    Ptr<Packet> packet = Create<Packet>(100);
    ++g_generated;

    const uint32_t deadlineStep = sequence % 5;
    uint32_t deadlineMs = 10 + (deadlineStep * 10);
    if (g_config.deadlineMode == "medium")
    {
        deadlineMs = 40 * (1 + deadlineStep);
    }
    else if (g_config.deadlineMode == "custom")
    {
        deadlineMs =
            (g_config.customMinDeadlineMs * (4 - deadlineStep) +
             g_config.customMaxDeadlineMs * deadlineStep) /
            4;
    }
    const uint64_t noDeadlinePacketsBefore =
        (static_cast<uint64_t>(sequence) * g_config.noDeadlinePercent + 50) / 100;
    const uint64_t noDeadlinePacketsThrough =
        (static_cast<uint64_t>(sequence + 1) * g_config.noDeadlinePercent + 50) / 100;
    const bool noDeadline =
        noDeadlinePacketsThrough > noDeadlinePacketsBefore;

    if (noDeadline)
    {
        ++g_generatedWithoutDeadline;
    }
    else
    {
        DeadlineTag deadlineTag;

        if (g_config.fairnessTest)
        {
            // Keep packets from expiring during the fairness experiment.
            deadlineTag.SetDeadline(
                Simulator::Now() + Seconds(3600));
        }
        else
        {
            // Preserve the original deadline-sensitive experiment.
            deadlineTag.SetDeadline(
                Simulator::Now() + MilliSeconds(deadlineMs));
        }

        packet->AddPacketTag(deadlineTag);
    }

    Ptr<QueueDiscItem> item =
        Create<ExampleQueueDiscItem>(
            packet,
            Address(),
            priority,
            flowId,
            importance);

    if (g_queueDisc->Enqueue(item))
    {
        ++g_enqueued;

        std::cout << "[Enqueue]"
                  << " seq=" << sequence
                  << " flow=" << flowId
                  << " priority=" << static_cast<uint32_t>(priority)
                  << " deadline=";
        if (noDeadline)
        {
            std::cout << "none";
        }
        else
        {
            std::cout << deadlineMs << " ms";
        }
        std::cout
                  << " time=" << Simulator::Now().GetMilliSeconds()
                  << " ms\n";
    }
    else
    {
        // Enqueue() returning false means this packet was rejected.
        ++g_enqueueDrops;
    }

    if (sequence + 1 < g_config.packetCount)
    {
        Simulator::Schedule(
            MilliSeconds(g_config.arrivalIntervalMs),
            &GeneratePacket,
            sequence + 1);
    }
}

// ------------------------------------------------------------
// Serve at most one packet per service interval.
// A null return may mean an expired packet was dropped.
// ------------------------------------------------------------
static void
ServePacket()
{
    if (Simulator::Now() >= MilliSeconds(g_config.durationMs))
    {
        return;
    }

    Ptr<QueueDiscItem> item = g_queueDisc->Dequeue();

    if (item)
    {
        ++g_transmitted;
        g_transmittedBytes += item->GetPacket()->GetSize();

        std::cout << "[Transmit]"
                  << " bytes=" << item->GetPacket()->GetSize()
                  << " time=" << Simulator::Now().GetMilliSeconds()
                  << " ms\n";
    }

    Simulator::Schedule(
        MilliSeconds(g_config.serviceIntervalMs),
        &ServePacket);
}

// ------------------------------------------------------------
// Write results to CSV
// ------------------------------------------------------------
static void
WriteResults()
{
    const double durationSeconds =
        static_cast<double>(g_config.durationMs) / 1000.0;

    const double averageDelayMs =
        g_queueDelaySamples == 0
            ? 0.0
            : static_cast<double>(g_totalQueueDelayNs) /
                  static_cast<double>(g_queueDelaySamples) / 1e6;

    const double throughputBps =
        durationSeconds <= 0.0
            ? 0.0
            : static_cast<double>(g_transmittedBytes) * 8.0 /
                  durationSeconds;

    const uint32_t totalDrops =
        g_enqueueDrops + g_expiredDrops;

    const uint32_t remainingInQueue = g_queueDisc->GetNPackets();

    bool writeHeader = false;

    {
        std::ifstream input(g_config.csvFile);

        if (!input.good() ||
            input.peek() == std::ifstream::traits_type::eof())
        {
            writeHeader = true;
        }
    }

    std::ofstream csv(g_config.csvFile, std::ios::app);

    if (!csv.is_open())
    {
        std::cerr << "Error: Could not open CSV file: "
                  << g_config.csvFile << '\n';
        return;
    }

    if (writeHeader)
    {
        csv << "mode,generated,enqueued,enqueue_drops,expired_drops,"
            "transmitted,remaining_in_queue,deadline_misses,"
            "avg_queue_delay_ms,throughput_bps,"
            "priority_0_tx,priority_1_tx,priority_2_tx,priority_3_tx,"
            "priority_4_tx,priority_5_tx,priority_6_tx,priority_7_tx\n";
    }

    csv << static_cast<uint32_t>(g_config.schedulingMode) << ','
        << g_generated << ','
        << g_enqueued << ','
        << g_enqueueDrops << ','
        << g_expiredDrops << ','
        << g_transmitted << ','
        << remainingInQueue << ','
        << g_queueDisc->GetDeadlineMissCount() << ','
        << std::fixed << std::setprecision(3)
        << averageDelayMs << ','
        << std::setprecision(2)
        << throughputBps << ','
        << g_transmittedByPriority[0] << ','
        << g_transmittedByPriority[1] << ','
        << g_transmittedByPriority[2] << ','
        << g_transmittedByPriority[3] << ','
        << g_transmittedByPriority[4] << ','
        << g_transmittedByPriority[5] << ','
        << g_transmittedByPriority[6] << ','
        << g_transmittedByPriority[7] << '\n';

    csv.close();

    std::cout << "\n========== EXPERIMENT RESULTS ==========\n"
              << "Scheduling mode: "
              << static_cast<uint32_t>(g_config.schedulingMode) << '\n'
              << "Enqueued: " << g_enqueued << '\n'
              << "Generated without deadline: "
              << g_generatedWithoutDeadline << '\n'
              << "Transmitted: " << g_transmitted << '\n'
              << "Enqueue drops: " << g_enqueueDrops << '\n'
              << "Expired drops: " << g_expiredDrops << '\n'
              << "Total drops: " << totalDrops << '\n'
              << "Deadline misses: "
              << g_queueDisc->GetDeadlineMissCount() << '\n'
              << std::fixed << std::setprecision(3)
              << "Average queue delay: "
              << averageDelayMs << " ms\n"
              << std::setprecision(2)
              << "Queue-level throughput: "
              << throughputBps << " bps\n"
              << "CSV file: " << g_config.csvFile << '\n'
              << "========================================\n";
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);

    cmd.AddValue(
        "schedulingMode",
        "0=Strict Priority, 1=EDF, 2=Hybrid, 3=WRR",
        g_config.schedulingMode);

    cmd.AddValue(
        "queueLimit",
        "Maximum number of packets in the queue",
        g_config.queueLimit);

    cmd.AddValue(
        "maxPriority",
        "Maximum packet priority (generated priorities range from 0 to this value; default 4, maximum 7)",
        g_config.maxPriority);

    cmd.AddValue(
        "noDeadlinePercent",
        "Approximate percentage of generated packets with no deadline (0-100; default 0)",
        g_config.noDeadlinePercent);

    cmd.AddValue(
        "deadlineMode",
        "Deadline mode: high=original 10-50 ms (default), medium=40-200 ms, custom=selected min/max",
        g_config.deadlineMode);

    cmd.AddValue(
        "customMinDeadlineMs",
        "Minimum deadline for custom mode, in milliseconds (1-60000)",
        g_config.customMinDeadlineMs);

    cmd.AddValue(
        "customMaxDeadlineMs",
        "Maximum deadline for custom mode, in milliseconds (1-60000)",
        g_config.customMaxDeadlineMs);

    cmd.AddValue(
        "expiredPolicy",
        "0=Drop expired packets, 1=Transmit expired packets",
        g_config.expiredPolicy);

    cmd.AddValue(
        "arrivalIntervalMs",
        "Packet arrival interval in milliseconds",
        g_config.arrivalIntervalMs);

    cmd.AddValue(
        "serviceIntervalMs",
        "Packet service interval in milliseconds",
        g_config.serviceIntervalMs);

    cmd.AddValue(
        "durationMs",
        "Experiment duration in milliseconds",
        g_config.durationMs);

    cmd.AddValue(
        "packetCount",
        "Number of packets to generate",
        g_config.packetCount);

    cmd.AddValue(
        "csvFile",
        "CSV output filename",
        g_config.csvFile);

    cmd.AddValue(
        "fairnessTest",
        "Enable controlled WRR fairness testing",
        g_config.fairnessTest);

    cmd.Parse(argc, argv);

    if (g_config.schedulingMode > 3 ||
        g_config.expiredPolicy > 1 ||
        g_config.maxPriority > 7 ||
        g_config.noDeadlinePercent > 100 ||
        (g_config.deadlineMode != "high" &&
         g_config.deadlineMode != "medium" &&
         g_config.deadlineMode != "custom") ||
        g_config.customMinDeadlineMs == 0 ||
        g_config.customMaxDeadlineMs > 60000 ||
        g_config.customMinDeadlineMs > g_config.customMaxDeadlineMs ||
        g_config.queueLimit == 0 ||
        g_config.arrivalIntervalMs == 0 ||
        g_config.serviceIntervalMs == 0 ||
        g_config.durationMs == 0 ||
        g_config.packetCount == 0)
    {
        std::cerr << "Invalid experiment configuration.\n";
        return 1;
    }

    // Create and configure the queue disc.
    g_queueDisc = CreateObject<FutureNetQueueDisc>();

    g_queueDisc->SetAttribute(
        "SchedulingMode",
        UintegerValue(g_config.schedulingMode));

    g_queueDisc->SetAttribute(
        "NumPriorityClasses",
        UintegerValue(g_config.maxPriority + 1));

    if (g_config.schedulingMode == 3)
    {
        std::ostringstream weights;
        for (uint32_t priority = 0; priority <= g_config.maxPriority; ++priority)
        {
            if (priority > 0)
            {
                weights << ',';
            }
            weights << (priority < 4 ? 4 - priority : 1);
        }
        g_queueDisc->SetAttribute("WrrWeights", StringValue(weights.str()));
    }

    g_queueDisc->SetAttribute(
        "QueueLimit",
        UintegerValue(g_config.queueLimit));

    g_queueDisc->SetAttribute(
        "ExpiredPacketPolicy",
        UintegerValue(g_config.expiredPolicy));

    // Connect the existing queue-disc trace sources.
    g_queueDisc->TraceConnectWithoutContext(
        "DeadlineMiss",
        MakeCallback(&PrintDeadlineMiss));

    g_queueDisc->TraceConnectWithoutContext(
        "ExpiredDrop",
        MakeCallback(&RecordExpiredDrop));

    g_queueDisc->TraceConnectWithoutContext(
        "ClassDequeued",
        MakeCallback(&RecordDequeued));

    g_queueDisc->Initialize();

    std::cout << "Starting FutureNet QoS experiment\n"
              << "Mode: "
              << static_cast<uint32_t>(g_config.schedulingMode) << '\n'
              << "Packet count: " << g_config.packetCount << '\n'
              << "Queue limit: " << g_config.queueLimit << '\n'
              << "Priorities: 0-" << g_config.maxPriority << '\n'
              << "Deadline mode: " << g_config.deadlineMode << '\n'
              << "No-deadline packet target: "
              << g_config.noDeadlinePercent << "%\n"
              << "Arrival interval: "
              << g_config.arrivalIntervalMs << " ms\n"
              << "Service interval: "
              << g_config.serviceIntervalMs << " ms\n"
              << "Duration: " << g_config.durationMs << " ms\n\n";

    if (g_config.deadlineMode == "custom")
    {
        std::cout << "Custom deadline range: "
                  << g_config.customMinDeadlineMs << '-'
                  << g_config.customMaxDeadlineMs << " ms\n\n";
    }

    // Schedule packet generation and periodic service.
    Simulator::ScheduleNow(&GeneratePacket, 0);
    Simulator::ScheduleNow(&ServePacket);

    Simulator::Stop(MilliSeconds(g_config.durationMs));
    Simulator::Run();

    WriteResults();

    Simulator::Destroy();

    return 0;
}
