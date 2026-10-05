#include "ns3/futurenet-queue-disc.h"
#include "ns3/futurenet-traffic-tag.h"
#include "ns3/futurenet-deadline-tag.h"
#include "ns3/packet.h"
#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

using namespace ns3;

/**
 * @brief Test item used by FutureNetQueueDisc tests.
 */
class FutureNetQueueDiscTestItem : public QueueDiscItem
{
  public:
    FutureNetQueueDiscTestItem(Ptr<Packet> packet, const Address& address, uint8_t priority);

    void AddHeader() override;
    bool Mark() override;
};

FutureNetQueueDiscTestItem::FutureNetQueueDiscTestItem(Ptr<Packet> packet,
                                                       const Address& address,
                                                       uint8_t priority)
    : QueueDiscItem(packet, address, 0)
{
    FutureNetTrafficTag tag;
    tag.SetPriority(priority);
    packet->ReplacePacketTag(tag);
}

void
FutureNetQueueDiscTestItem::AddHeader()
{
}

bool
FutureNetQueueDiscTestItem::Mark()
{
    return false;
}

class FutureNetQueueDiscNoTagTestItem : public QueueDiscItem
{
  public:
    FutureNetQueueDiscNoTagTestItem(Ptr<Packet> packet, const Address& address);

    void AddHeader() override;
    bool Mark() override;
};

FutureNetQueueDiscNoTagTestItem::FutureNetQueueDiscNoTagTestItem(
    Ptr<Packet> packet,
    const Address& address)
    : QueueDiscItem(packet, address, 0)
{
}

void
FutureNetQueueDiscNoTagTestItem::AddHeader()
{
}

bool
FutureNetQueueDiscNoTagTestItem::Mark()
{
    return false;
}

uint32_t g_expiredDropFlowId = 0;
uint8_t g_expiredDropPriority = 255;
uint32_t g_expiredDropCount = 0;

void
ExpiredDropCallback(uint32_t flowId, uint8_t priority)
{
    g_expiredDropFlowId = flowId;
    g_expiredDropPriority = priority;
    g_expiredDropCount++;
}

uint8_t g_classDequeuedPriority = 255;
Time g_classDequeuedSojournTime = Time::Min();
uint32_t g_classDequeuedCount = 0;

void
ClassDequeuedCallback(uint8_t priority, Time sojournTime)
{
    g_classDequeuedPriority = priority;
    g_classDequeuedSojournTime = sojournTime;
    g_classDequeuedCount++;
}

/**
 * @brief Test FutureNetQueueDisc construction.
 */
class FutureNetQueueDiscTestCase : public TestCase
{
  public:
    FutureNetQueueDiscTestCase()
        : TestCase("Test FutureNetQueueDisc construction and internal queues")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        NS_TEST_ASSERT_MSG_NE(queueDisc,
                              nullptr,
                              "QueueDisc was not created");

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNInternalQueues(),
                              0,
                              "Internal queues should not exist before initialization");
        queueDisc->SetAttribute("QueueLimit", UintegerValue(2));

        queueDisc->Initialize();

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNInternalQueues(),
                              4,
                              "FutureNetQueueDisc should create four priority queues");
        
        Ptr<Packet> packet1 = Create<Packet>();
        Ptr<Packet> packet2 = Create<Packet>();
        Ptr<Packet> packet3 = Create<Packet>();

        Ptr<QueueDiscItem> item1 =
            Create<FutureNetQueueDiscTestItem>(packet1, Address(), 0);

        Ptr<QueueDiscItem> item2 =
            Create<FutureNetQueueDiscTestItem>(packet2, Address(), 0);

        Ptr<QueueDiscItem> item3 =
            Create<FutureNetQueueDiscTestItem>(packet3, Address(), 0);

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(item1),
            true,
            "First packet should be enqueued");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(item2),
            true,
            "Second packet should be enqueued");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(item3),
            false,
            "Third packet should be dropped when QueueLimit is 2");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            2,
            "Queue should contain only two packets");

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetDeadlineMissCount(),
                              0,
                              "Initial deadline miss count should be zero");
    }
};

/**
 * @brief Test strict priority scheduling.
 */
class FutureNetQueueDiscPriorityTestCase : public TestCase
{
  public:
    FutureNetQueueDiscPriorityTestCase()
        : TestCase("Test FutureNetQueueDisc strict priority ordering")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        queueDisc->Initialize();

        Address address;

        Ptr<Packet> packetPriority3 = Create<Packet>(100);
        Ptr<Packet> packetPriority1 = Create<Packet>(100);
        Ptr<Packet> packetPriority0 = Create<Packet>(100);
        Ptr<Packet> packetPriority2 = Create<Packet>(100);

        Ptr<QueueDiscItem> itemPriority3 =
            Create<FutureNetQueueDiscTestItem>(packetPriority3, address, 3);

        Ptr<QueueDiscItem> itemPriority1 =
            Create<FutureNetQueueDiscTestItem>(packetPriority1, address, 1);

        Ptr<QueueDiscItem> itemPriority0 =
            Create<FutureNetQueueDiscTestItem>(packetPriority0, address, 0);

        Ptr<QueueDiscItem> itemPriority2 =
            Create<FutureNetQueueDiscTestItem>(packetPriority2, address, 2);

        // Enqueue in non-priority order.
        queueDisc->Enqueue(itemPriority3);
        queueDisc->Enqueue(itemPriority1);
        queueDisc->Enqueue(itemPriority0);
        queueDisc->Enqueue(itemPriority2);

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNPackets(),
                              4,
                              "QueueDisc should contain four packets");

        Ptr<QueueDiscItem> item;

        // Priority 0 must be dequeued first.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(item,
                              nullptr,
                              "First dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(item->GetPacket()->GetUid(),
                              packetPriority0->GetUid(),
                              "Priority 0 packet should be dequeued first");

        // Priority 1 must be dequeued second.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(item,
                              nullptr,
                              "Second dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(item->GetPacket()->GetUid(),
                              packetPriority1->GetUid(),
                              "Priority 1 packet should be dequeued second");

        // Priority 2 must be dequeued third.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(item,
                              nullptr,
                              "Third dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(item->GetPacket()->GetUid(),
                              packetPriority2->GetUid(),
                              "Priority 2 packet should be dequeued third");

        // Priority 3 must be dequeued last.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(item,
                              nullptr,
                              "Fourth dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(item->GetPacket()->GetUid(),
                              packetPriority3->GetUid(),
                              "Priority 3 packet should be dequeued fourth");

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNPackets(),
                              0,
                              "QueueDisc should be empty after all packets are dequeued");
    }
};

/**
 * @brief Test EDF scheduling.
 */
class FutureNetQueueDiscEdfTestCase : public TestCase
{
  public:
    FutureNetQueueDiscEdfTestCase()
        : TestCase("Test FutureNetQueueDisc EDF ordering")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        // SchedulingMode:
        // 0 = StrictPriority
        // 1 = EDF
        // 2 = Hybrid
        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->SetAttributeFailSafe("QueueLimit", UintegerValue(50)),
            true,
            "QueueLimit attribute should exist");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->SetAttributeFailSafe("DefaultPriority", UintegerValue(2)),
            true,
            "DefaultPriority attribute should exist");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->SetAttributeFailSafe("NumPriorityClasses", UintegerValue(4)),
            true,
            "NumPriorityClasses attribute should exist");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->SetAttributeFailSafe("SchedulingMode", UintegerValue(1)),
            true,
            "SchedulingMode attribute should exist");

        queueDisc->Initialize();

        Address address;

        // Create three packets.
        Ptr<Packet> packet30ms = Create<Packet>(100);
        Ptr<Packet> packet10ms = Create<Packet>(100);
        Ptr<Packet> packet20ms = Create<Packet>(100);

        // Add deadlines.
        DeadlineTag deadline30ms;
        deadline30ms.SetDeadline(MilliSeconds(30));
        packet30ms->AddPacketTag(deadline30ms);

        DeadlineTag deadline10ms;
        deadline10ms.SetDeadline(MilliSeconds(10));
        packet10ms->AddPacketTag(deadline10ms);

        DeadlineTag deadline20ms;
        deadline20ms.SetDeadline(MilliSeconds(20));
        packet20ms->AddPacketTag(deadline20ms);

        // Give all packets the same priority so that EDF
        // is responsible for the ordering.
        Ptr<QueueDiscItem> item30ms =
            Create<FutureNetQueueDiscTestItem>(packet30ms, address, 0);

        Ptr<QueueDiscItem> item10ms =
            Create<FutureNetQueueDiscTestItem>(packet10ms, address, 0);

        Ptr<QueueDiscItem> item20ms =
            Create<FutureNetQueueDiscTestItem>(packet20ms, address, 0);

        // Enqueue in a deliberately different order.
        queueDisc->Enqueue(item30ms);
        queueDisc->Enqueue(item10ms);
        queueDisc->Enqueue(item20ms);

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNPackets(),
                              3,
                              "QueueDisc should contain three packets");

        Ptr<QueueDiscItem> item;

        // 10 ms deadline should be dequeued first.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(item,
                              nullptr,
                              "First dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(item->GetPacket()->GetUid(),
                              packet10ms->GetUid(),
                              "10 ms deadline packet should be dequeued first");

        // 20 ms deadline should be dequeued second.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(item,
                              nullptr,
                              "Second dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(item->GetPacket()->GetUid(),
                              packet20ms->GetUid(),
                              "20 ms deadline packet should be dequeued second");

        // 30 ms deadline should be dequeued last.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(item,
                              nullptr,
                              "Third dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(item->GetPacket()->GetUid(),
                              packet30ms->GetUid(),
                              "30 ms deadline packet should be dequeued last");

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNPackets(),
                              0,
                              "QueueDisc should be empty after all packets are dequeued");
        // Test EDF DoPeek selects the earliest deadline without removing the packet
        Ptr<FutureNetQueueDisc> peekQueueDisc =
            CreateObject<FutureNetQueueDisc>();

        peekQueueDisc->SetAttribute("SchedulingMode", UintegerValue(1));
        peekQueueDisc->Initialize();

        Ptr<Packet> latePacket = Create<Packet>();

        DeadlineTag lateDeadline;
        lateDeadline.SetDeadline(MilliSeconds(20));
        latePacket->AddPacketTag(lateDeadline);

        FutureNetTrafficTag lateTrafficTag;
        lateTrafficTag.SetPriority(0);
        lateTrafficTag.SetFlowId(1);
        latePacket->AddPacketTag(lateTrafficTag);

        Ptr<QueueDiscItem> lateItem =
            Create<FutureNetQueueDiscTestItem>(latePacket, Address(), 0);

        Ptr<Packet> earlyPacket = Create<Packet>();

        DeadlineTag earlyDeadline;
        earlyDeadline.SetDeadline(MilliSeconds(10));
        earlyPacket->AddPacketTag(earlyDeadline);

        FutureNetTrafficTag earlyTrafficTag;
        earlyTrafficTag.SetPriority(1);
        earlyTrafficTag.SetFlowId(2);
        earlyPacket->AddPacketTag(earlyTrafficTag);

        Ptr<QueueDiscItem> earlyItem =
            Create<FutureNetQueueDiscTestItem>(earlyPacket, Address(), 1);

        NS_TEST_ASSERT_MSG_EQ(peekQueueDisc->Enqueue(lateItem),
                            true,
                            "Late packet should be enqueued");

        NS_TEST_ASSERT_MSG_EQ(peekQueueDisc->Enqueue(earlyItem),
                            true,
                            "Early packet should be enqueued");

        Ptr<const QueueDiscItem> peekedItem = peekQueueDisc->Peek();

        NS_TEST_ASSERT_MSG_NE(peekedItem,
                            nullptr,
                            "Peek should return a packet");

        NS_TEST_ASSERT_MSG_EQ(peekedItem->GetPacket()->GetUid(),
                            earlyPacket->GetUid(),
                            "EDF Peek should return the earliest-deadline packet");

        NS_TEST_ASSERT_MSG_EQ(peekQueueDisc->GetNPackets(),
                            2,
                            "Peek should not remove the packet");
    }
};

/**
 * @brief Test Hybrid scheduling.
 *
 * Hybrid scheduling:
 * 1. Lower priority number is served first.
 * 2. Within the same priority, earlier deadline is served first.
 */
class FutureNetQueueDiscHybridTestCase : public TestCase
{
  public:
    FutureNetQueueDiscHybridTestCase()
        : TestCase("Test FutureNetQueueDisc Hybrid ordering")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->SetAttributeFailSafe("SchedulingMode", UintegerValue(2)),
            true,
            "SchedulingMode attribute should exist");

        queueDisc->Initialize();

        Address address;

        // Priority 0, deadline 30 ms
        Ptr<Packet> packetP0D30 = Create<Packet>(100);
        DeadlineTag deadlineP0D30;
        deadlineP0D30.SetDeadline(MilliSeconds(30));
        packetP0D30->AddPacketTag(deadlineP0D30);

        // Priority 1, deadline 1 ms
        Ptr<Packet> packetP1D1 = Create<Packet>(100);
        DeadlineTag deadlineP1D1;
        deadlineP1D1.SetDeadline(MilliSeconds(1));
        packetP1D1->AddPacketTag(deadlineP1D1);

        // Priority 2, deadline 20 ms
        Ptr<Packet> packetP2D20 = Create<Packet>(100);
        DeadlineTag deadlineP2D20;
        deadlineP2D20.SetDeadline(MilliSeconds(20));
        packetP2D20->AddPacketTag(deadlineP2D20);

        // Priority 2, deadline 10 ms
        Ptr<Packet> packetP2D10 = Create<Packet>(100);
        DeadlineTag deadlineP2D10;
        deadlineP2D10.SetDeadline(MilliSeconds(10));
        packetP2D10->AddPacketTag(deadlineP2D10);

        Ptr<QueueDiscItem> itemP0D30 =
            Create<FutureNetQueueDiscTestItem>(packetP0D30, address, 0);

        Ptr<QueueDiscItem> itemP1D1 =
            Create<FutureNetQueueDiscTestItem>(packetP1D1, address, 1);

        Ptr<QueueDiscItem> itemP2D20 =
            Create<FutureNetQueueDiscTestItem>(packetP2D20, address, 2);

        Ptr<QueueDiscItem> itemP2D10 =
            Create<FutureNetQueueDiscTestItem>(packetP2D10, address, 2);

        // Enqueue in a deliberately mixed order.
        queueDisc->Enqueue(itemP2D20);
        queueDisc->Enqueue(itemP0D30);
        queueDisc->Enqueue(itemP2D10);
        queueDisc->Enqueue(itemP1D1);

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            4,
            "QueueDisc should contain four packets");

        Ptr<QueueDiscItem> item;

        // Priority 0 must win, even though its deadline is 30 ms.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            item,
            nullptr,
            "First dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(
            item->GetPacket()->GetUid(),
            packetP0D30->GetUid(),
            "Priority 0 packet should be dequeued first");

        // Priority 1 must come before priority 2,
        // even though its deadline is earlier.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            item,
            nullptr,
            "Second dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(
            item->GetPacket()->GetUid(),
            packetP1D1->GetUid(),
            "Priority 1 packet should be dequeued second");

        // Within priority 2, EDF should select 10 ms first.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            item,
            nullptr,
            "Third dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(
            item->GetPacket()->GetUid(),
            packetP2D10->GetUid(),
            "10 ms deadline should be dequeued before 20 ms within priority 2");

        // Remaining priority 2 packet.
        item = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            item,
            nullptr,
            "Fourth dequeued item should not be null");

        NS_TEST_ASSERT_MSG_EQ(
            item->GetPacket()->GetUid(),
            packetP2D20->GetUid(),
            "20 ms deadline packet should be dequeued last");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            0,
            "QueueDisc should be empty after all packets are dequeued");
    }
};

/**
 * @brief Test expired packet handling.
 */
class FutureNetQueueDiscDeadlineTestCase : public TestCase
{
  public:
    FutureNetQueueDiscDeadlineTestCase()
        : TestCase("Test FutureNetQueueDisc expired packet handling")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        // Strict Priority mode.
        queueDisc->Initialize();

        g_expiredDropFlowId = 0;
        g_expiredDropPriority = 255;
        g_expiredDropCount = 0;

        g_classDequeuedPriority = 255;
        g_classDequeuedSojournTime = Time::Min();
        g_classDequeuedCount = 0;

        queueDisc->TraceConnectWithoutContext(
            "ExpiredDrop",
            MakeCallback(&ExpiredDropCallback));

        queueDisc->TraceConnectWithoutContext(
            "ClassDequeued",
            MakeCallback(&ClassDequeuedCallback));

        Address address;

        // Create an already-expired packet.
        Ptr<Packet> expiredPacket = Create<Packet>(100);

        DeadlineTag deadlineTag;
        deadlineTag.SetDeadline(MilliSeconds(0));
        expiredPacket->AddPacketTag(deadlineTag);

        Ptr<QueueDiscItem> expiredItem =
            Create<FutureNetQueueDiscTestItem>(
                expiredPacket, address, 0);

        queueDisc->Enqueue(expiredItem);

        // Move simulation time past the deadline.
        Simulator::Schedule(MilliSeconds(1), []() {});

        Simulator::Run();

        Ptr<QueueDiscItem> item = queueDisc->Dequeue();

        // Default policy is Drop, so the expired packet should be dropped.
        NS_TEST_ASSERT_MSG_EQ(
            item,
            nullptr,
            "Expired packet should be dropped");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetDeadlineMissCount(),
            1,
            "Deadline miss count should be one");
        
        NS_TEST_ASSERT_MSG_EQ(
            g_expiredDropCount,
            1,
            "ExpiredDrop trace should fire once");

        NS_TEST_ASSERT_MSG_EQ(
            g_expiredDropFlowId,
            0,
            "ExpiredDrop flow ID should be zero");

        NS_TEST_ASSERT_MSG_EQ(
            g_expiredDropPriority,
            0,
            "ExpiredDrop priority should be zero");

        g_classDequeuedPriority = 255;
        g_classDequeuedSojournTime = Time::Min();
        g_classDequeuedCount = 0;

        Ptr<FutureNetQueueDisc> classDequeuedQueueDisc =
            CreateObject<FutureNetQueueDisc>();

        classDequeuedQueueDisc->SetAttribute(
            "SchedulingMode",
            UintegerValue(0));

        classDequeuedQueueDisc->Initialize();

        classDequeuedQueueDisc->TraceConnectWithoutContext(
            "ClassDequeued",
            MakeCallback(&ClassDequeuedCallback));

        Ptr<Packet> classDequeuedPacket = Create<Packet>();

        FutureNetTrafficTag classDequeuedTag;
        classDequeuedTag.SetPriority(0);
        classDequeuedTag.SetFlowId(1);
        classDequeuedPacket->AddPacketTag(classDequeuedTag);

        Ptr<QueueDiscItem> classDequeuedItem = Create<FutureNetQueueDiscTestItem>(
                                                            classDequeuedPacket,
                                                            Address(),
                                                            0);

        classDequeuedQueueDisc->Enqueue(classDequeuedItem);

        Simulator::Schedule(
            MilliSeconds(5),
            [classDequeuedQueueDisc]()
            {
                classDequeuedQueueDisc->Dequeue();
            });

        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(
            g_classDequeuedCount,
            1,
            "ClassDequeued trace should fire once");

        NS_TEST_ASSERT_MSG_EQ(
            g_classDequeuedPriority,
            0,
            "ClassDequeued priority should be zero");

        NS_TEST_ASSERT_MSG_EQ(
            g_classDequeuedSojournTime,
            MilliSeconds(5),
            "ClassDequeued sojourn time should be 5 ms");

        // Test packet without FutureNetTrafficTag uses DefaultPriority
        Ptr<FutureNetQueueDisc> noTagQueueDisc = CreateObject<FutureNetQueueDisc>();

        noTagQueueDisc->SetAttribute("SchedulingMode", UintegerValue(0));
        noTagQueueDisc->SetAttribute("DefaultPriority", UintegerValue(2));
        noTagQueueDisc->Initialize();

        Ptr<Packet> noTagPacket = Create<Packet>();
        Ptr<QueueDiscItem> noTagItem =
            Create<FutureNetQueueDiscNoTagTestItem>(noTagPacket, Address());

        NS_TEST_ASSERT_MSG_EQ(noTagQueueDisc->Enqueue(noTagItem),
                            true,
                            "Packet without tag should be enqueued");

        Ptr<QueueDiscItem> dequeuedNoTag = noTagQueueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(dequeuedNoTag,
                            nullptr,
                            "Packet without tag should be dequeued");

        FutureNetTrafficTag trafficTag;
        NS_TEST_ASSERT_MSG_EQ(dequeuedNoTag->GetPacket()->PeekPacketTag(trafficTag),
                            false,
                            "Packet should not have a FutureNetTrafficTag");
        
        // Test DefaultDeadline for packet without DeadlineTag
        Ptr<FutureNetQueueDisc> defaultDeadlineQueueDisc =
            CreateObject<FutureNetQueueDisc>();

        defaultDeadlineQueueDisc->SetAttribute("SchedulingMode",
                                                UintegerValue(0));
        defaultDeadlineQueueDisc->SetAttribute("DefaultDeadline",
                                                TimeValue(MilliSeconds(10)));
        defaultDeadlineQueueDisc->Initialize();

        Ptr<Packet> defaultDeadlinePacket = Create<Packet>();

        Ptr<QueueDiscItem> defaultDeadlineItem =
            Create<FutureNetQueueDiscNoTagTestItem>(
                defaultDeadlinePacket, Address());

        NS_TEST_ASSERT_MSG_EQ(defaultDeadlineQueueDisc->Enqueue(defaultDeadlineItem),
                            true,
                            "Packet without deadline should be enqueued");

        DeadlineTag defaultDeadlineTag;

        NS_TEST_ASSERT_MSG_EQ(
            defaultDeadlineItem->GetPacket()->PeekPacketTag(defaultDeadlineTag),
            true,
            "DefaultDeadline should add a DeadlineTag");

        NS_TEST_ASSERT_MSG_EQ(defaultDeadlineTag.GetDeadline(),
                            MilliSeconds(10),
                            "Default deadline should be 10 ms");
        
        // Test DeadlineCheckInterval
        Ptr<FutureNetQueueDisc> intervalQueueDisc =
            CreateObject<FutureNetQueueDisc>();

        intervalQueueDisc->SetAttribute("SchedulingMode", UintegerValue(0));
        intervalQueueDisc->SetAttribute("DeadlineCheckInterval",
                                        TimeValue(MilliSeconds(10)));
        intervalQueueDisc->Initialize();

        Ptr<Packet> intervalPacket = Create<Packet>();

        DeadlineTag intervalDeadline;
        intervalDeadline.SetDeadline(MilliSeconds(5));
        intervalPacket->AddPacketTag(intervalDeadline);

        FutureNetTrafficTag intervalTrafficTag;
        intervalTrafficTag.SetPriority(0);
        intervalTrafficTag.SetFlowId(10);
        intervalPacket->AddPacketTag(intervalTrafficTag);

        Ptr<QueueDiscItem> intervalItem =
            Create<FutureNetQueueDiscTestItem>(intervalPacket, Address(), 0);

        NS_TEST_ASSERT_MSG_EQ(intervalQueueDisc->Enqueue(intervalItem),
                            true,
                            "Packet should be enqueued");

        // Advance beyond the packet deadline.
        Simulator::Schedule(MilliSeconds(6), [&intervalQueueDisc]() {
            intervalQueueDisc->Dequeue();
        });

        Simulator::Run();

        Ptr<QueueDiscItem> intervalResult = intervalQueueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_EQ(intervalResult,
                            nullptr,
                            "Expired packet should be dropped");

        Simulator::Destroy();

    }
};

/**
 * @brief Test expired packet transmit policy.
 */
class FutureNetQueueDiscExpiredTransmitTestCase : public TestCase
{
  public:
    FutureNetQueueDiscExpiredTransmitTestCase()
        : TestCase("Test FutureNetQueueDisc expired packet transmit policy")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        // Set ExpiredPacketPolicy = 1 (Transmit).
        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->SetAttributeFailSafe(
                "ExpiredPacketPolicy",
                UintegerValue(1)),
            true,
            "ExpiredPacketPolicy attribute should exist");

        queueDisc->Initialize();

        Address address;

        // Create an expired packet.
        Ptr<Packet> expiredPacket = Create<Packet>(100);

        DeadlineTag deadlineTag;
        deadlineTag.SetDeadline(MilliSeconds(0));
        expiredPacket->AddPacketTag(deadlineTag);

        Ptr<QueueDiscItem> expiredItem =
            Create<FutureNetQueueDiscTestItem>(
                expiredPacket, address, 0);

        queueDisc->Enqueue(expiredItem);

        // Move simulation time past the deadline.
        Simulator::Schedule(MilliSeconds(1), []() {});

        Simulator::Run();

        Ptr<QueueDiscItem> item = queueDisc->Dequeue();

        // Expired packet should be transmitted.
        NS_TEST_ASSERT_MSG_NE(
            item,
            nullptr,
            "Expired packet should be transmitted");

        NS_TEST_ASSERT_MSG_EQ(
            item->GetPacket()->GetUid(),
            expiredPacket->GetUid(),
            "The expired packet should be returned");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetDeadlineMissCount(),
            1,
            "Deadline miss count should be one");

        Simulator::Destroy();
    }
};

/**
 * @brief FutureNetQueueDisc test suite.
 */
class FutureNetQueueDiscTestSuite : public TestSuite
{
  public:
    FutureNetQueueDiscTestSuite()
        : TestSuite("futurenet-queue-disc", TestSuite::Type::UNIT)
    {
        AddTestCase(new FutureNetQueueDiscTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscPriorityTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscEdfTestCase,
                    TestCase::Duration::QUICK);
        
        AddTestCase(new FutureNetQueueDiscHybridTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscDeadlineTestCase,
                    TestCase::Duration::QUICK);
        
        AddTestCase(new FutureNetQueueDiscExpiredTransmitTestCase,
            TestCase::Duration::QUICK);
    }

};

static FutureNetQueueDiscTestSuite futurenetQueueDiscTestSuite;