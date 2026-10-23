#include "ns3/futurenet-queue-disc.h"
#include "ns3/futurenet-traffic-tag.h"
#include "ns3/futurenet-deadline-tag.h"
#include "ns3/packet.h"
#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <string>

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

FutureNetQueueDiscTestItem::FutureNetQueueDiscTestItem(
    Ptr<Packet> packet,
    const Address& address,
    uint8_t priority)
    : QueueDiscItem(packet, address, 0)
{
    FutureNetTrafficTag tag;

    // Preserve an existing traffic tag, including its flow ID.
    if (!packet->PeekPacketTag(tag))
    {
        tag.SetPriority(priority);
    }
    else
    {
        // Keep the flow ID and importance already set by the test.
        tag.SetPriority(priority);
    }

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

static uint32_t g_deadlineMissFlowId = 0;
static uint8_t g_deadlineMissPriority = 0;
static Time g_deadlineMissLateness = Seconds(0);
static uint32_t g_deadlineMissTraceCount = 0;

static void
DeadlineMissTraceCallback(uint32_t flowId,
                          uint8_t priority,
                          Time lateness)
{
    g_deadlineMissFlowId = flowId;
    g_deadlineMissPriority = priority;
    g_deadlineMissLateness = lateness;
    ++g_deadlineMissTraceCount;
}

static uint32_t g_expiredDropTraceFlowId = 0;
static uint8_t g_expiredDropTracePriority = 0;
static uint32_t g_expiredDropTraceCount = 0;

static void
ExpiredDropTraceCallback(uint32_t flowId, uint8_t priority)
{
    g_expiredDropTraceFlowId = flowId;
    g_expiredDropTracePriority = priority;
    ++g_expiredDropTraceCount;
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
 * @brief Test that QueueLimit applies globally across priority queues.
 */
class FutureNetQueueDiscGlobalQueueLimitTestCase : public TestCase
{
  public:
    FutureNetQueueDiscGlobalQueueLimitTestCase()
        : TestCase("Test global queue limit across priority classes")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        queueDisc->SetAttribute("QueueLimit", UintegerValue(3));
        queueDisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        queueDisc->SetAttribute("SchedulingMode", UintegerValue(0));

        queueDisc->Initialize();

        // Enqueue packets into three different priority classes.
        for (uint8_t priority = 0; priority < 3; ++priority)
        {
            Ptr<QueueDiscItem> item =
                Create<FutureNetQueueDiscTestItem>(
                    Create<Packet>(100),
                    Address(),
                    priority);

            NS_TEST_ASSERT_MSG_EQ(
                queueDisc->Enqueue(item),
                true,
                "Packet should be accepted while below global QueueLimit");
        }

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            3,
            "Queue should contain three packets across priority classes");

        // The global limit is reached, so another packet must be rejected.
        Ptr<QueueDiscItem> overflowItem =
            Create<FutureNetQueueDiscTestItem>(
                Create<Packet>(100),
                Address(),
                3);

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(overflowItem),
            false,
            "Fourth packet should be rejected at the global queue limit");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            3,
            "Rejected packet must not increase the total queue size");

        // Dequeue one packet to free a slot.
        Ptr<QueueDiscItem> dequeuedItem = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            dequeuedItem,
            nullptr,
            "Dequeue should return a packet");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            2,
            "Queue should contain two packets after dequeue");

        // A new packet should now be accepted.
        Ptr<QueueDiscItem> replacementItem =
            Create<FutureNetQueueDiscTestItem>(
                Create<Packet>(100),
                Address(),
                3);

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(replacementItem),
            true,
            "Packet should be accepted after a queue slot is freed");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            3,
            "Queue should return to the global limit");

        Simulator::Destroy();
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


class FutureNetQueueDiscDefaultPriorityTestCase : public TestCase
{
  public:
    FutureNetQueueDiscDefaultPriorityTestCase()
        : TestCase("Untagged packets use the configured default priority")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(0));
        qdisc->SetAttribute("DefaultPriority", UintegerValue(2));
        qdisc->Initialize();

        Ptr<Packet> untaggedPacket = Create<Packet>(100);
        Ptr<QueueDiscItem> untaggedItem =
            Create<FutureNetQueueDiscNoTagTestItem>(
                untaggedPacket, Address());

        Ptr<Packet> priority3Packet = Create<Packet>(100);
        Ptr<QueueDiscItem> priority3Item =
            Create<FutureNetQueueDiscTestItem>(
                priority3Packet, Address(), 3);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(priority3Item), true,
            "Priority 3 packet should enqueue");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(untaggedItem), true,
            "Untagged packet should enqueue");

        Ptr<QueueDiscItem> first = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            first, nullptr,
            "First dequeue should return a packet");

        NS_TEST_ASSERT_MSG_EQ(
            first->GetPacket()->GetUid(),
            untaggedPacket->GetUid(),
            "Default-priority-2 packet should precede priority 3");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscPriorityPeekTestCase : public TestCase
{
  public:
    FutureNetQueueDiscPriorityPeekTestCase()
        : TestCase("Test FutureNetQueueDisc strict priority Peek")
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

        // Enqueue packets in non-priority order.
        queueDisc->Enqueue(itemPriority3);
        queueDisc->Enqueue(itemPriority1);
        queueDisc->Enqueue(itemPriority0);
        queueDisc->Enqueue(itemPriority2);

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNPackets(),
                              4,
                              "QueueDisc should contain four packets");

        // Peek should return the highest-priority packet (priority 0).
        Ptr<const QueueDiscItem> peekedItem = queueDisc->Peek();

        NS_TEST_ASSERT_MSG_NE(peekedItem,
                              nullptr,
                              "Peek should return a non-null item");

        NS_TEST_ASSERT_MSG_EQ(peekedItem->GetPacket()->GetUid(),
                              packetPriority0->GetUid(),
                              "Peek should return the priority 0 packet");

        // Peek must not remove any packets.
        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNPackets(),
                              4,
                              "Peek should not change the packet count");

        // Dequeue should return the same packet that Peek returned.
        Ptr<QueueDiscItem> dequeuedItem = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(dequeuedItem,
                              nullptr,
                              "Dequeue should return a non-null item");

        NS_TEST_ASSERT_MSG_EQ(dequeuedItem->GetPacket()->GetUid(),
                              peekedItem->GetPacket()->GetUid(),
                              "Dequeue should return the packet previously peeked");

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNPackets(),
                              3,
                              "Exactly one packet should be removed by Dequeue");
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
        Ptr<QueueDiscItem> dequeuedAfterPeek = peekQueueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(dequeuedAfterPeek,
                            nullptr,
                            "Dequeue should return a packet after Peek");

        NS_TEST_ASSERT_MSG_EQ(dequeuedAfterPeek->GetPacket()->GetUid(),
                            peekedItem->GetPacket()->GetUid(),
                            "Dequeue should return the same packet that Peek returned");

        NS_TEST_ASSERT_MSG_EQ(peekQueueDisc->GetNPackets(),
                            1,
                            "Dequeue should remove exactly one packet");
    }
};


/**
 * @brief Test FIFO ordering for packets with equal EDF deadlines.
 */
class FutureNetQueueDiscEdfEqualDeadlineTestCase : public TestCase
{
  public:
    FutureNetQueueDiscEdfEqualDeadlineTestCase()
        : TestCase("EDF preserves FIFO order for equal deadlines")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        queueDisc->SetAttribute("SchedulingMode", UintegerValue(1));
        queueDisc->Initialize();

        // Both packets have exactly the same absolute deadline.
        Ptr<Packet> packet1 = Create<Packet>(100);
        Ptr<Packet> packet2 = Create<Packet>(100);

        DeadlineTag deadline1;
        deadline1.SetDeadline(MilliSeconds(20));
        packet1->AddPacketTag(deadline1);

        DeadlineTag deadline2;
        deadline2.SetDeadline(MilliSeconds(20));
        packet2->AddPacketTag(deadline2);

        Ptr<QueueDiscItem> item1 =
            Create<FutureNetQueueDiscTestItem>(
                packet1, Address(), 1);

        Ptr<QueueDiscItem> item2 =
            Create<FutureNetQueueDiscTestItem>(
                packet2, Address(), 1);

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(item1),
            true,
            "First packet should be enqueued");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(item2),
            true,
            "Second packet should be enqueued");

        Ptr<const QueueDiscItem> peeked = queueDisc->Peek();

        NS_TEST_ASSERT_MSG_NE(
            peeked,
            nullptr,
            "Peek should return the first packet");

        NS_TEST_ASSERT_MSG_EQ(
            peeked->GetPacket()->GetUid(),
            packet1->GetUid(),
            "First inserted packet should be at the EDF queue head");

        Ptr<QueueDiscItem> dequeued1 = queueDisc->Dequeue();
        Ptr<QueueDiscItem> dequeued2 = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            dequeued1,
            nullptr,
            "First dequeue should return a packet");

        NS_TEST_ASSERT_MSG_NE(
            dequeued2,
            nullptr,
            "Second dequeue should return a packet");

        NS_TEST_ASSERT_MSG_EQ(
            dequeued1->GetPacket()->GetUid(),
            packet1->GetUid(),
            "Equal-deadline packets should preserve FIFO order");

        NS_TEST_ASSERT_MSG_EQ(
            dequeued2->GetPacket()->GetUid(),
            packet2->GetUid(),
            "Second inserted packet should be dequeued second");

        Simulator::Destroy();
    }
};


/**
 * @brief Test EDF ordering when one packet has no deadline metadata.
 */
class FutureNetQueueDiscEdfMissingDeadlineTestCase : public TestCase
{
  public:
    FutureNetQueueDiscEdfMissingDeadlineTestCase()
        : TestCase("EDF prioritizes valid deadlines over missing deadlines")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        queueDisc->SetAttribute("SchedulingMode", UintegerValue(1));

        // Keep the missing-deadline packet without an assigned default
        // deadline, so the EDF queue treats it as Time::Max().
        queueDisc->SetAttribute(
            "DefaultDeadline",
            TimeValue(Time::Min()));

        queueDisc->Initialize();

        // Enqueue the packet without a DeadlineTag first.
        Ptr<Packet> noDeadlinePacket = Create<Packet>(100);

        Ptr<QueueDiscItem> noDeadlineItem =
            Create<FutureNetQueueDiscNoTagTestItem>(
                noDeadlinePacket, Address());

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(noDeadlineItem),
            true,
            "Packet without deadline metadata should be enqueued");

        // This packet has a finite deadline and should be served first.
        Ptr<Packet> deadlinePacket = Create<Packet>(100);

        DeadlineTag deadline;
        deadline.SetDeadline(MilliSeconds(20));
        deadlinePacket->AddPacketTag(deadline);

        Ptr<QueueDiscItem> deadlineItem =
            Create<FutureNetQueueDiscTestItem>(
                deadlinePacket, Address(), 1);

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(deadlineItem),
            true,
            "Packet with a deadline should be enqueued");

        Ptr<const QueueDiscItem> peeked = queueDisc->Peek();

        NS_TEST_ASSERT_MSG_NE(
            peeked,
            nullptr,
            "Peek should return a packet");

        NS_TEST_ASSERT_MSG_EQ(
            peeked->GetPacket()->GetUid(),
            deadlinePacket->GetUid(),
            "Packet with a finite deadline should be first");

        Ptr<QueueDiscItem> dequeued1 = queueDisc->Dequeue();
        Ptr<QueueDiscItem> dequeued2 = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            dequeued1,
            nullptr,
            "First dequeue should return a packet");

        NS_TEST_ASSERT_MSG_NE(
            dequeued2,
            nullptr,
            "Second dequeue should return a packet");

        NS_TEST_ASSERT_MSG_EQ(
            dequeued1->GetPacket()->GetUid(),
            deadlinePacket->GetUid(),
            "Finite-deadline packet should be dequeued first");

        NS_TEST_ASSERT_MSG_EQ(
            dequeued2->GetPacket()->GetUid(),
            noDeadlinePacket->GetUid(),
            "Packet without deadline metadata should be dequeued last");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscEdfMissingDeadlineFifoTestCase : public TestCase
{
  public:
    FutureNetQueueDiscEdfMissingDeadlineFifoTestCase()
        : TestCase("EDF preserves FIFO order for packets without deadlines")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(1));
        qdisc->SetAttribute("DefaultDeadline", TimeValue(Time::Min()));
        qdisc->Initialize();

        Ptr<Packet> packet1 = Create<Packet>(100);
        Ptr<Packet> packet2 = Create<Packet>(100);
        Ptr<Packet> packet3 = Create<Packet>(100);

        Ptr<QueueDiscItem> item1 =
            Create<FutureNetQueueDiscNoTagTestItem>(packet1, Address());
        Ptr<QueueDiscItem> item2 =
            Create<FutureNetQueueDiscNoTagTestItem>(packet2, Address());
        Ptr<QueueDiscItem> item3 =
            Create<FutureNetQueueDiscNoTagTestItem>(packet3, Address());

        NS_TEST_ASSERT_MSG_EQ(qdisc->Enqueue(item1), true,
                              "First packet should enqueue");
        NS_TEST_ASSERT_MSG_EQ(qdisc->Enqueue(item2), true,
                              "Second packet should enqueue");
        NS_TEST_ASSERT_MSG_EQ(qdisc->Enqueue(item3), true,
                              "Third packet should enqueue");

        Ptr<QueueDiscItem> dequeued1 = qdisc->Dequeue();
        Ptr<QueueDiscItem> dequeued2 = qdisc->Dequeue();
        Ptr<QueueDiscItem> dequeued3 = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_EQ(dequeued1->GetPacket()->GetUid(),
                              packet1->GetUid(),
                              "First packet should dequeue first");
        NS_TEST_ASSERT_MSG_EQ(dequeued2->GetPacket()->GetUid(),
                              packet2->GetUid(),
                              "Second packet should dequeue second");
        NS_TEST_ASSERT_MSG_EQ(dequeued3->GetPacket()->GetUid(),
                              packet3->GetUid(),
                              "Third packet should dequeue third");

        Simulator::Destroy();
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

        Simulator::Destroy();
    }
};


/**
 * @brief Test Peek behavior in Hybrid scheduling mode.
 */
class FutureNetQueueDiscHybridPeekTestCase : public TestCase
{
  public:
    FutureNetQueueDiscHybridPeekTestCase()
        : TestCase("Test FutureNetQueueDisc Hybrid Peek")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> queueDisc =
            CreateObject<FutureNetQueueDisc>();

        queueDisc->SetAttribute("SchedulingMode", UintegerValue(2));
        queueDisc->Initialize();

        Address address;

        // Priority 0, deadline 30 ms.
        Ptr<Packet> packetP0D30 = Create<Packet>(100);
        DeadlineTag deadlineP0D30;
        deadlineP0D30.SetDeadline(MilliSeconds(30));
        packetP0D30->AddPacketTag(deadlineP0D30);

        // Priority 0, deadline 10 ms (should be selected first).
        Ptr<Packet> packetP0D10 = Create<Packet>(100);
        DeadlineTag deadlineP0D10;
        deadlineP0D10.SetDeadline(MilliSeconds(10));
        packetP0D10->AddPacketTag(deadlineP0D10);

        // Priority 1, deadline 1 ms.
        Ptr<Packet> packetP1D1 = Create<Packet>(100);
        DeadlineTag deadlineP1D1;
        deadlineP1D1.SetDeadline(MilliSeconds(1));
        packetP1D1->AddPacketTag(deadlineP1D1);

        Ptr<QueueDiscItem> itemP0D30 =
            Create<FutureNetQueueDiscTestItem>(
                packetP0D30, address, 0);

        Ptr<QueueDiscItem> itemP0D10 =
            Create<FutureNetQueueDiscTestItem>(
                packetP0D10, address, 0);

        Ptr<QueueDiscItem> itemP1D1 =
            Create<FutureNetQueueDiscTestItem>(
                packetP1D1, address, 1);

        // Enqueue in a deliberately mixed order.
        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(itemP0D30),
            true,
            "Priority 0 packet with 30 ms deadline should enqueue");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(itemP1D1),
            true,
            "Priority 1 packet should enqueue");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->Enqueue(itemP0D10),
            true,
            "Priority 0 packet with 10 ms deadline should enqueue");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            3,
            "QueueDisc should contain three packets");

        // Peek should select priority 0 first, then EDF within priority 0.
        Ptr<const QueueDiscItem> peekedItem = queueDisc->Peek();

        NS_TEST_ASSERT_MSG_NE(
            peekedItem,
            nullptr,
            "Peek should return a packet");

        NS_TEST_ASSERT_MSG_EQ(
            peekedItem->GetPacket()->GetUid(),
            packetP0D10->GetUid(),
            "Hybrid Peek should return the earliest-deadline packet in priority 0");

        // Peek must not remove a packet.
        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            3,
            "Peek should not change the packet count");

        // Dequeue should return the same packet that Peek returned.
        Ptr<QueueDiscItem> dequeuedItem = queueDisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            dequeuedItem,
            nullptr,
            "Dequeue should return a packet after Peek");

        NS_TEST_ASSERT_MSG_EQ(
            dequeuedItem->GetPacket()->GetUid(),
            peekedItem->GetPacket()->GetUid(),
            "Dequeue should return the same packet that Peek returned");

        NS_TEST_ASSERT_MSG_EQ(
            queueDisc->GetNPackets(),
            2,
            "Dequeue should remove exactly one packet");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscHybridEqualDeadlineTestCase : public TestCase
{
  public:
    FutureNetQueueDiscHybridEqualDeadlineTestCase()
        : TestCase("Hybrid preserves FIFO order for equal deadlines in one class")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(2));
        qdisc->Initialize();


        Ptr<Packet> packet1 = Create<Packet>(100);
        Ptr<Packet> packet2 = Create<Packet>(100);
        Ptr<Packet> packet3 = Create<Packet>(100);
        const Time deadline = MilliSeconds(20);

        DeadlineTag tag;
        tag.SetDeadline(deadline);
        packet1->AddPacketTag(tag);
        packet2->AddPacketTag(tag);
        packet3->AddPacketTag(tag);

        Ptr<QueueDiscItem> item1 =
            Create<FutureNetQueueDiscTestItem>(
                packet1, Address(), 1);
        Ptr<QueueDiscItem> item2 =
            Create<FutureNetQueueDiscTestItem>(
                packet2, Address(), 1);
        Ptr<QueueDiscItem> item3 =
            Create<FutureNetQueueDiscTestItem>(
                packet3, Address(), 1);

        NS_TEST_ASSERT_MSG_EQ(qdisc->Enqueue(item1), true,
                              "First packet should enqueue");
        NS_TEST_ASSERT_MSG_EQ(qdisc->Enqueue(item2), true,
                              "Second packet should enqueue");
        NS_TEST_ASSERT_MSG_EQ(qdisc->Enqueue(item3), true,
                              "Third packet should enqueue");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Dequeue()->GetPacket()->GetUid(),
            packet1->GetUid(),
            "First packet should dequeue first");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Dequeue()->GetPacket()->GetUid(),
            packet2->GetUid(),
            "Second packet should dequeue second");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Dequeue()->GetPacket()->GetUid(),
            packet3->GetUid(),
            "Third packet should dequeue third");

        Simulator::Destroy();
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

class FutureNetQueueDiscWrrTestCase : public TestCase
{
public:
    FutureNetQueueDiscWrrTestCase()
        : TestCase("Weighted Round Robin serves packets according to class weights")
    {
    }

private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("3,2,1,1"));

        qdisc->Initialize();

        // Add enough packets to every class to keep all queues non-empty.
        for (uint8_t priority = 0; priority < 4; ++priority)
        {
            for (uint32_t i = 0; i < 10; ++i)
            {
                Ptr<QueueDiscItem> item =
                        Create<FutureNetQueueDiscTestItem>(
                            Create<Packet>(100),
                            Address(),
                            priority);

                NS_TEST_ASSERT_MSG_EQ(
                    qdisc->Enqueue(item),
                    true,
                    "Packet enqueue should succeed");
            }
        }

        // Expected WRR sequence for weights 3,2,1,1.
        const std::vector<uint8_t> expected = {
            0, 0, 0, 1, 1, 2, 3,
            0, 0, 0, 1, 1, 2, 3
        };

        for (uint32_t i = 0; i < expected.size(); ++i)
        {
            Ptr<const QueueDiscItem> item = qdisc->Dequeue();

            NS_TEST_ASSERT_MSG_NE(
                item,
                nullptr,
                "Dequeue should return a packet");

            FutureNetTrafficTag tag;
            NS_TEST_ASSERT_MSG_EQ(
                item->GetPacket()->PeekPacketTag(tag),
                true,
                "Dequeued packet should contain a traffic tag");

            NS_TEST_ASSERT_MSG_EQ(
                tag.GetPriority(),
                expected[i],
                "Dequeued priority should follow WRR weights");
        }

        Simulator::Destroy();
    }
};



class FutureNetQueueDiscWrrPeekTestCase : public TestCase
{
public:
    FutureNetQueueDiscWrrPeekTestCase()
        : TestCase("WRR Peek does not remove the next packet")
    {
    }

private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("3,2,1,1"));

        qdisc->Initialize();

        // Enqueue one packet in each class.
        for (uint8_t priority = 0; priority < 4; ++priority)
        {
            Ptr<QueueDiscItem> item =
                Create<FutureNetQueueDiscTestItem>(
                    Create<Packet>(100),
                    Address(),
                    priority);

            NS_TEST_ASSERT_MSG_EQ(
                qdisc->Enqueue(item),
                true,
                "Packet enqueue should succeed");
        }

        // Repeated Peek calls must not change scheduler state.
        Ptr<const QueueDiscItem> firstPeek = qdisc->Peek();
        Ptr<const QueueDiscItem> secondPeek = qdisc->Peek();

        NS_TEST_ASSERT_MSG_NE(
            firstPeek,
            nullptr,
            "First Peek should return a packet");

        NS_TEST_ASSERT_MSG_NE(
            secondPeek,
            nullptr,
            "Second Peek should return a packet");

        NS_TEST_ASSERT_MSG_EQ(
            firstPeek->GetPacket()->GetUid(),
            secondPeek->GetPacket()->GetUid(),
            "Repeated Peek calls should return the same packet");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            4,
            "Repeated Peek calls should not remove packets");

        Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            dequeued,
            nullptr,
            "Dequeue should return a packet");

        NS_TEST_ASSERT_MSG_EQ(
            firstPeek->GetPacket()->GetUid(),
            dequeued->GetPacket()->GetUid(),
            "Peek should predict the next dequeued packet");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscWrrPeekAfterWeightTestCase : public TestCase
{
public:
    FutureNetQueueDiscWrrPeekAfterWeightTestCase()
        : TestCase("WRR Peek predicts the next class after weight is exhausted")
    {
    }

private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("3,2,1,1"));
        qdisc->Initialize();

        // Class 0 has more packets than its weight.
        // Class 1 has one packet.
        for (uint8_t priority : {0, 0, 0, 0, 1})
        {
            Ptr<QueueDiscItem> item =
                Create<FutureNetQueueDiscTestItem>(
                    Create<Packet>(100), Address(), priority);

            NS_TEST_ASSERT_MSG_EQ(
                qdisc->Enqueue(item),
                true,
                "Packet enqueue should succeed");
        }

        // Consume class 0's three-packet WRR turn.
        for (uint32_t i = 0; i < 3; ++i)
        {
            Ptr<QueueDiscItem> item = qdisc->Dequeue();

            NS_TEST_ASSERT_MSG_NE(
                item,
                nullptr,
                "Expected a packet from class 0");

            FutureNetTrafficTag tag;
            item->GetPacket()->PeekPacketTag(tag);

            NS_TEST_ASSERT_MSG_EQ(
                tag.GetPriority(),
                0,
                "The first three packets should be from class 0");
        }

        // Class 0 still has a packet, but its WRR turn is exhausted.
        // Peek should predict class 1, which Dequeue should then serve.
        Ptr<const QueueDiscItem> peeked = qdisc->Peek();

        NS_TEST_ASSERT_MSG_NE(
            peeked,
            nullptr,
            "Peek should return the next packet");

        FutureNetTrafficTag peekTag;
        peeked->GetPacket()->PeekPacketTag(peekTag);

        NS_TEST_ASSERT_MSG_EQ(
            peekTag.GetPriority(),
            1,
            "Peek should predict class 1 after class 0's weight is exhausted");

        Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            dequeued,
            nullptr,
            "Dequeue should return a packet");

        FutureNetTrafficTag dequeuedTag;
        dequeued->GetPacket()->PeekPacketTag(dequeuedTag);

        NS_TEST_ASSERT_MSG_EQ(
            dequeuedTag.GetPriority(),
            1,
            "Dequeue should serve the same class predicted by Peek");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscWrrEmptyClassTestCase : public TestCase
{
public:
    FutureNetQueueDiscWrrEmptyClassTestCase()
        : TestCase("WRR skips empty classes and serves them after refill")
    {
    }

private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("3,2,1,1"));

        qdisc->Initialize();

        // Only classes 0 and 2 initially contain packets.
        for (uint8_t priority : {0, 0, 0, 2})
        {
            Ptr<QueueDiscItem> item =
                Create<FutureNetQueueDiscTestItem>(
                    Create<Packet>(100), Address(), priority);

            NS_TEST_ASSERT_MSG_EQ(
                qdisc->Enqueue(item),
                true,
                "Initial packet enqueue should succeed");
        }

        // Class 0 uses its full weight, then empty class 1 is skipped.
        for (uint8_t expectedPriority : {0, 0, 0, 2})
        {
            Ptr<const QueueDiscItem> peeked = qdisc->Peek();

            NS_TEST_ASSERT_MSG_NE(
                peeked,
                nullptr,
                "Peek should return the next available packet");

            FutureNetTrafficTag peekTag;
            NS_TEST_ASSERT_MSG_EQ(
                peeked->GetPacket()->PeekPacketTag(peekTag),
                true,
                "Peeked packet should have a traffic tag");

            NS_TEST_ASSERT_MSG_EQ(
                peekTag.GetPriority(),
                expectedPriority,
                "Peek should predict the next available class");

            Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

            NS_TEST_ASSERT_MSG_NE(
                dequeued,
                nullptr,
                "Dequeue should return a packet");

            FutureNetTrafficTag dequeuedTag;
            NS_TEST_ASSERT_MSG_EQ(
                dequeued->GetPacket()->PeekPacketTag(dequeuedTag),
                true,
                "Dequeued packet should have a traffic tag");

            NS_TEST_ASSERT_MSG_EQ(
                dequeuedTag.GetPriority(),
                expectedPriority,
                "WRR should skip empty classes");
        }

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            0,
            "All initial packets should have been dequeued");

        // Refill class 1 and verify it can receive service.
        Ptr<QueueDiscItem> refillItem =
            Create<FutureNetQueueDiscTestItem>(
                Create<Packet>(100), Address(), 1);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(refillItem),
            true,
            "Refill packet should be accepted");

        Ptr<const QueueDiscItem> peeked = qdisc->Peek();

        NS_TEST_ASSERT_MSG_NE(
            peeked,
            nullptr,
            "Peek should find the refilled packet");

        FutureNetTrafficTag peekTag;
        NS_TEST_ASSERT_MSG_EQ(
            peeked->GetPacket()->PeekPacketTag(peekTag),
            true,
            "Refilled packet should have a traffic tag");

        NS_TEST_ASSERT_MSG_EQ(
            peekTag.GetPriority(),
            1,
            "Peek should identify the refilled class");

        Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            dequeued,
            nullptr,
            "Refilled packet should be dequeued");

        FutureNetTrafficTag dequeuedTag;
        NS_TEST_ASSERT_MSG_EQ(
            dequeued->GetPacket()->PeekPacketTag(dequeuedTag),
            true,
            "Dequeued refill packet should have a traffic tag");

        NS_TEST_ASSERT_MSG_EQ(
            dequeuedTag.GetPriority(),
            1,
            "Refilled class should receive service");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscWrrUnderfilledClassTestCase : public TestCase
{
  public:
    FutureNetQueueDiscWrrUnderfilledClassTestCase()
        : TestCase("WRR serves other classes when one class has fewer packets than its weight")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("3,2,1,1"));
        qdisc->Initialize();

        // Class 0 has only one packet; class 1 has three.
        for (uint8_t priority : {0, 1, 1, 1})
        {
            Ptr<QueueDiscItem> item =
                Create<FutureNetQueueDiscTestItem>(
                    Create<Packet>(100), Address(), priority);

            NS_TEST_ASSERT_MSG_EQ(
                qdisc->Enqueue(item),
                true,
                "Packet enqueue should succeed");
        }

        // Class 0 is served first, then class 1 must receive service.
        for (uint8_t expectedPriority : {0, 1, 1, 1})
        {
            Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

            NS_TEST_ASSERT_MSG_NE(
                dequeued,
                nullptr,
                "WRR should return a packet");

            FutureNetTrafficTag tag;
            NS_TEST_ASSERT_MSG_EQ(
                dequeued->GetPacket()->PeekPacketTag(tag),
                true,
                "Dequeued packet should have a traffic tag");

            NS_TEST_ASSERT_MSG_EQ(
                tag.GetPriority(),
                expectedPriority,
                "WRR should skip the exhausted class and serve class 1");
        }

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            0,
            "All packets should have been dequeued");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscWrrMidRoundRefillTestCase : public TestCase
{
  public:
    FutureNetQueueDiscWrrMidRoundRefillTestCase()
        : TestCase("WRR serves a refilled class during an ongoing round")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("3,2,1,1"));
        qdisc->Initialize();

        // Start with two packets in class 0 and no packets in class 1.
        for (uint32_t i = 0; i < 2; ++i)
        {
            Ptr<QueueDiscItem> item =
                Create<FutureNetQueueDiscTestItem>(
                    Create<Packet>(100), Address(), 0);

            NS_TEST_ASSERT_MSG_EQ(
                qdisc->Enqueue(item),
                true,
                "Class 0 packet enqueue should succeed");
        }

        // First packet comes from class 0.
        Ptr<QueueDiscItem> first = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            first,
            nullptr,
            "First dequeue should return a packet");

        FutureNetTrafficTag firstTag;
        first->GetPacket()->PeekPacketTag(firstTag);

        NS_TEST_ASSERT_MSG_EQ(
            firstTag.GetPriority(),
            0,
            "First packet should come from class 0");

        // Refill class 1 before class 0 has used its full weight of 3.
        Ptr<QueueDiscItem> refill =
            Create<FutureNetQueueDiscTestItem>(
                Create<Packet>(100), Address(), 1);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(refill),
            true,
            "Class 1 refill should succeed");

        // Class 0 still has its turn and should receive its second packet.
        Ptr<QueueDiscItem> second = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            second,
            nullptr,
            "Second dequeue should return a packet");

        FutureNetTrafficTag secondTag;
        second->GetPacket()->PeekPacketTag(secondTag);

        NS_TEST_ASSERT_MSG_EQ(
            secondTag.GetPriority(),
            0,
            "Class 0 should continue its turn");

        // Once class 0 is empty, WRR should serve the refilled class 1.
        Ptr<QueueDiscItem> third = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            third,
            nullptr,
            "Third dequeue should return the refilled packet");

        FutureNetTrafficTag thirdTag;
        third->GetPacket()->PeekPacketTag(thirdTag);

        NS_TEST_ASSERT_MSG_EQ(
            thirdTag.GetPriority(),
            1,
            "Refilled class 1 should receive service");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            0,
            "All packets should have been dequeued");

        Simulator::Destroy();
    }
};



/**
 * @brief Test WRR continues after dropping an expired packet.
 */
class FutureNetQueueDiscWrrExpiredPacketTestCase : public TestCase
{
  public:
    FutureNetQueueDiscWrrExpiredPacketTestCase()
        : TestCase("WRR continues after dropping an expired packet")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("1,1,1,1"));
        qdisc->SetAttribute("ExpiredPacketPolicy", UintegerValue(0));

        qdisc->Initialize();

        // Class 0 contains an expired packet.
        Ptr<Packet> expiredPacket = Create<Packet>(100);

        DeadlineTag expiredDeadline;
        expiredDeadline.SetDeadline(MilliSeconds(0));
        expiredPacket->AddPacketTag(expiredDeadline);

        Ptr<QueueDiscItem> expiredItem =
            Create<FutureNetQueueDiscTestItem>(
                expiredPacket, Address(), 0);

        // Class 1 contains a packet that has not expired.
        Ptr<Packet> validPacket = Create<Packet>(100);

        DeadlineTag validDeadline;
        validDeadline.SetDeadline(MilliSeconds(10));
        validPacket->AddPacketTag(validDeadline);

        Ptr<QueueDiscItem> validItem =
            Create<FutureNetQueueDiscTestItem>(
                validPacket, Address(), 1);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(expiredItem),
            true,
            "Expired packet should initially be enqueued");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(validItem),
            true,
            "Valid packet should be enqueued");

        // Advance time beyond the expired packet's deadline.
        Simulator::Schedule(MilliSeconds(1), []() {});
        Simulator::Run();

        // WRR should drop the expired packet and continue to class 1.
        Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            dequeued,
            nullptr,
            "WRR should return the valid packet after dropping the expired one");

        NS_TEST_ASSERT_MSG_EQ(
            dequeued->GetPacket()->GetUid(),
            validPacket->GetUid(),
            "The valid class 1 packet should be returned");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetDeadlineMissCount(),
            1,
            "Exactly one deadline miss should be recorded");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            0,
            "Both packets should have been removed or dropped");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscWrrPeekExpiredPacketTestCase : public TestCase
{
  public:
    FutureNetQueueDiscWrrPeekExpiredPacketTestCase()
        : TestCase("WRR Peek does not drop expired packets")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("1,1,1,1"));
        qdisc->SetAttribute("ExpiredPacketPolicy", UintegerValue(0));
        qdisc->Initialize();

        // Enqueue an already-expired packet in class 0.
        Ptr<Packet> expiredPacket = Create<Packet>(100);

        DeadlineTag expiredDeadline;
        expiredDeadline.SetDeadline(MilliSeconds(0));
        expiredPacket->AddPacketTag(expiredDeadline);

        Ptr<QueueDiscItem> expiredItem =
            Create<FutureNetQueueDiscTestItem>(
                expiredPacket, Address(), 0);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(expiredItem),
            true,
            "Expired packet should initially be enqueued");

        // Advance simulation time beyond the deadline.
        Simulator::Schedule(MilliSeconds(1), []() {});
        Simulator::Run();

        // Peek should inspect the candidate without dropping it.
        Ptr<const QueueDiscItem> peeked = qdisc->Peek();

        NS_TEST_ASSERT_MSG_NE(
            peeked,
            nullptr,
            "Peek should return the queued candidate");

        NS_TEST_ASSERT_MSG_EQ(
            peeked->GetPacket()->GetUid(),
            expiredPacket->GetUid(),
            "Peek should return the expired head packet");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetDeadlineMissCount(),
            0,
            "Peek must not record a deadline miss");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            1,
            "Peek must not remove the expired packet");

        // Dequeue performs the actual deadline check and drop.
        Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_EQ(
            dequeued,
            nullptr,
            "Dequeue should drop the only expired packet");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetDeadlineMissCount(),
            1,
            "Dequeue should record exactly one deadline miss");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            0,
            "The expired packet should have been removed");

        Simulator::Destroy();
    }
};


/**
 * @brief Test DeadlineMiss trace reports the correct packet information.
 */
class FutureNetQueueDiscDeadlineMissTraceTestCase : public TestCase
{
  public:
    FutureNetQueueDiscDeadlineMissTraceTestCase()
        : TestCase("DeadlineMiss trace reports correct packet information")
    {
    }

  private:
    void DoRun() override
    {
        g_deadlineMissFlowId = 0;
        g_deadlineMissPriority = 0;
        g_deadlineMissLateness = Seconds(0);
        g_deadlineMissTraceCount = 0;

        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("1,1,1,1"));
        qdisc->SetAttribute("ExpiredPacketPolicy", UintegerValue(0));
        qdisc->SetAttribute("EnableDeadlineTracing", BooleanValue(true));

        qdisc->Initialize();

        bool connected = qdisc->TraceConnectWithoutContext(
            "DeadlineMiss",
            MakeCallback(&DeadlineMissTraceCallback));

        NS_TEST_ASSERT_MSG_EQ(
            connected,
            true,
            "Should connect to the DeadlineMiss trace");

        Ptr<Packet> packet = Create<Packet>(100);

        DeadlineTag deadlineTag;
        deadlineTag.SetDeadline(MilliSeconds(5));
        packet->AddPacketTag(deadlineTag);

        FutureNetTrafficTag trafficTag;
        trafficTag.SetPriority(2);
        trafficTag.SetFlowId(123);
        packet->AddPacketTag(trafficTag);

        FutureNetTrafficTag verifyTag;

        NS_TEST_ASSERT_MSG_EQ(
            packet->PeekPacketTag(verifyTag),
            true,
            "Traffic tag should exist before enqueue");

        NS_TEST_ASSERT_MSG_EQ(
            verifyTag.GetFlowId(),
            123,
            "Flow ID should be 123 before enqueue");

        Ptr<QueueDiscItem> item =
            Create<FutureNetQueueDiscTestItem>(
                packet, Address(), 2);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(item),
            true,
            "Packet should be enqueued successfully");

        // Advance simulation time beyond the packet deadline.
        Simulator::Schedule(MilliSeconds(10), []() {});
        Simulator::Run();

        // Dequeue triggers the deadline check.
        Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_EQ(
            dequeued,
            nullptr,
            "Expired packet should be dropped");

        NS_TEST_ASSERT_MSG_EQ(
            g_deadlineMissTraceCount,
            1,
            "DeadlineMiss trace should fire exactly once");

        NS_TEST_ASSERT_MSG_EQ(
            g_deadlineMissFlowId,
            123,
            "Trace should report the correct flow ID");

        NS_TEST_ASSERT_MSG_EQ(
            static_cast<uint32_t>(g_deadlineMissPriority),
            2,
            "Trace should report the correct priority");

        NS_TEST_ASSERT_MSG_EQ(
            g_deadlineMissLateness,
            MilliSeconds(5),
            "Lateness should be 5 ms (10 ms now minus 5 ms deadline)");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetDeadlineMissCount(),
            1,
            "Deadline miss count should be one");

        Simulator::Destroy();
    }
};

class FutureNetQueueDiscExpiredDropTraceTestCase : public TestCase
{
  public:
    FutureNetQueueDiscExpiredDropTraceTestCase()
        : TestCase("ExpiredDrop trace reports correct flow ID and priority")
    {
    }

  private:
    void DoRun() override
    {
        g_expiredDropTraceFlowId = 0;
        g_expiredDropTracePriority = 0;
        g_expiredDropTraceCount = 0;

        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("1,1,1,1"));
        qdisc->SetAttribute("ExpiredPacketPolicy", UintegerValue(0));
        qdisc->SetAttribute("EnableDeadlineTracing", BooleanValue(true));

        qdisc->Initialize();

        bool connected = qdisc->TraceConnectWithoutContext(
            "ExpiredDrop",
            MakeCallback(&ExpiredDropTraceCallback));

        NS_TEST_ASSERT_MSG_EQ(
            connected, true, "Should connect to the ExpiredDrop trace");

        Ptr<Packet> packet = Create<Packet>(100);

        DeadlineTag deadlineTag;
        deadlineTag.SetDeadline(MilliSeconds(5));
        packet->AddPacketTag(deadlineTag);

        FutureNetTrafficTag trafficTag;
        trafficTag.SetPriority(2);
        trafficTag.SetFlowId(456);
        packet->AddPacketTag(trafficTag);

        Ptr<QueueDiscItem> item =
            Create<FutureNetQueueDiscTestItem>(
                packet, Address(), 2);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(item), true, "Packet should be enqueued");

        // Move simulation time past the deadline.
        Simulator::Schedule(MilliSeconds(10), []() {});
        Simulator::Run();

        Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_EQ(
            dequeued, nullptr, "Expired packet should be dropped");

        NS_TEST_ASSERT_MSG_EQ(
            g_expiredDropTraceCount,
            1,
            "ExpiredDrop trace should fire exactly once");

        NS_TEST_ASSERT_MSG_EQ(
            g_expiredDropTraceFlowId,
            456,
            "ExpiredDrop should report flow ID 456");

        NS_TEST_ASSERT_MSG_EQ(
            static_cast<uint32_t>(g_expiredDropTracePriority),
            2,
            "ExpiredDrop should report priority 2");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetDeadlineMissCount(),
            1,
            "Exactly one deadline miss should be recorded");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscDefaultDeadlineTestCase : public TestCase
{
  public:
    FutureNetQueueDiscDefaultDeadlineTestCase()
        : TestCase("DefaultDeadline is applied to packets without a deadline")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(0));
        qdisc->SetAttribute("DefaultDeadline", TimeValue(MilliSeconds(20)));

        qdisc->Initialize();

        Ptr<Packet> packet = Create<Packet>(100);

        // Add a traffic tag, but no DeadlineTag.
        FutureNetTrafficTag trafficTag;
        trafficTag.SetPriority(0);
        trafficTag.SetFlowId(789);
        packet->AddPacketTag(trafficTag);

        Ptr<QueueDiscItem> item =
            Create<FutureNetQueueDiscTestItem>(
                packet, Address(), 0);

        DeadlineTag initialTag;

        NS_TEST_ASSERT_MSG_EQ(
            packet->PeekPacketTag(initialTag),
            false,
            "Packet should initially have no deadline tag");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(item),
            true,
            "Packet should enqueue successfully");

        DeadlineTag resultTag;
        NS_TEST_ASSERT_MSG_EQ(
            packet->PeekPacketTag(resultTag),
            true,
            "DefaultDeadline should add a deadline tag");

        NS_TEST_ASSERT_MSG_EQ(
            resultTag.GetDeadline(),
            MilliSeconds(20),
            "Deadline should match the configured default");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscWrrMinimumWeightsTestCase : public TestCase
{
  public:
    FutureNetQueueDiscWrrMinimumWeightsTestCase()
        : TestCase("WRR accepts minimum valid weights")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("WrrWeights", StringValue("1,1,1,1"));

        qdisc->Initialize();

        // All four weights are valid and the configuration initializes.
        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            0,
            "New WRR queue should be empty");

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscWrrValidWeightsTestCase : public TestCase
{
  public:
    FutureNetQueueDiscWrrValidWeightsTestCase()
        : TestCase("WRR accepts valid weight configurations")
    {
    }

  private:
    void DoRun() override
    {
        const std::vector<std::string> configurations = {
            "1,1,1,1",
            "4,3,2,1"
        };

        for (const auto& weights : configurations)
        {
            Ptr<FutureNetQueueDisc> qdisc =
                CreateObject<FutureNetQueueDisc>();

            qdisc->SetAttribute("SchedulingMode", UintegerValue(3));
            qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
            qdisc->SetAttribute("WrrWeights", StringValue(weights));

            qdisc->Initialize();

            NS_TEST_ASSERT_MSG_EQ(
                qdisc->GetNPackets(),
                0,
                "New WRR queue should be empty");

            qdisc->Dispose();
        }

        Simulator::Destroy();
    }
};


class FutureNetQueueDiscWrrInvalidWeightsTestCase : public TestCase
{
  public:
    FutureNetQueueDiscWrrInvalidWeightsTestCase()
        : TestCase("WRR rejects invalid weight configurations")
    {
    }

  private:
    void DoRun() override
    {
        const std::vector<std::string> invalidWeights = {
            "0,2,1,1",
            "-1,2,1,1",
            "2,x,1,1",
            "2,1,1",
            "2,1,1,1,1",
            "2,1,1,1,"
        };

        for (const auto& weights : invalidWeights)
        {
            pid_t child = fork();

            NS_TEST_ASSERT_MSG_NE(
                child,
                -1,
                "Could not create child process for weights: " << weights);

            if (child == 0)
            {
                Ptr<FutureNetQueueDisc> qdisc =
                    CreateObject<FutureNetQueueDisc>();

                qdisc->SetAttribute(
                    "SchedulingMode", UintegerValue(3));
                qdisc->SetAttribute(
                    "NumPriorityClasses", UintegerValue(4));
                qdisc->SetAttribute(
                    "WrrWeights", StringValue(weights));

                qdisc->Initialize();

                // Reaching here means invalid weights were accepted.
                _exit(0);
            }

            int status = 0;
            pid_t result = waitpid(child, &status, 0);

            NS_TEST_ASSERT_MSG_EQ(
                result,
                child,
                "Failed to wait for child process");

            bool rejected =
                WIFSIGNALED(status) ||
                (WIFEXITED(status) && WEXITSTATUS(status) != 0);

            NS_TEST_ASSERT_MSG_EQ(
                rejected,
                true,
                "Invalid WRR weights were not rejected: " << weights);
        }

        Simulator::Destroy();
    }
};



class FutureNetQueueDiscExpiredDropFreesCapacityTestCase : public TestCase
{
  public:
    FutureNetQueueDiscExpiredDropFreesCapacityTestCase()
        : TestCase("Dropping an expired packet frees queue capacity")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FutureNetQueueDisc> qdisc =
            CreateObject<FutureNetQueueDisc>();

        qdisc->SetAttribute("QueueLimit", UintegerValue(2));
        qdisc->SetAttribute("NumPriorityClasses", UintegerValue(4));
        qdisc->SetAttribute("SchedulingMode", UintegerValue(0));
        qdisc->SetAttribute("ExpiredPacketPolicy", UintegerValue(0));

        qdisc->Initialize();

        // Enqueue an expired packet in priority class 0.
        Ptr<Packet> expiredPacket = Create<Packet>(100);

        DeadlineTag expiredDeadline;
        expiredDeadline.SetDeadline(MilliSeconds(5));
        expiredPacket->AddPacketTag(expiredDeadline);

        Ptr<QueueDiscItem> expiredItem =
            Create<FutureNetQueueDiscTestItem>(
                expiredPacket, Address(), 0);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(expiredItem),
            true,
            "Expired packet should initially be accepted");

        // Fill the second slot with a packet without a deadline.
        Ptr<QueueDiscItem> secondItem =
            Create<FutureNetQueueDiscTestItem>(
                Create<Packet>(100), Address(), 1);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(secondItem),
            true,
            "Second packet should be accepted");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            2,
            "Queue should be full");

        // Advance beyond the first packet's deadline.
        Simulator::Schedule(MilliSeconds(10), []() {});
        Simulator::Run();

        // Strict priority checks class 0 first and drops the expired packet.
        Ptr<QueueDiscItem> dequeued = qdisc->Dequeue();

        NS_TEST_ASSERT_MSG_NE(
            dequeued,
            nullptr,
            "Dequeue should continue and return the valid packet");

        NS_TEST_ASSERT_MSG_EQ(
            dequeued->GetPacket()->GetUid(),
            secondItem->GetPacket()->GetUid(),
            "The valid packet should be returned after the expired packet is dropped");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            0,
            "Both packets should be removed or dropped");

        // Verify that the freed capacity can be used again.
        Ptr<QueueDiscItem> replacementItem =
            Create<FutureNetQueueDiscTestItem>(
                Create<Packet>(100), Address(), 2);

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->Enqueue(replacementItem),
            true,
            "A new packet should be accepted after the expired packet is dropped");

        NS_TEST_ASSERT_MSG_EQ(
            qdisc->GetNPackets(),
            1,
            "Queue should contain the replacement packet");

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

        AddTestCase(new FutureNetQueueDiscDefaultPriorityTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscPriorityPeekTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscEdfTestCase,
                    TestCase::Duration::QUICK);
        
        AddTestCase(new FutureNetQueueDiscHybridTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscHybridPeekTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscHybridEqualDeadlineTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscDeadlineTestCase,
                    TestCase::Duration::QUICK);
        
        AddTestCase(new FutureNetQueueDiscExpiredTransmitTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrTestCase(),
                    TestSuite::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscGlobalQueueLimitTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscEdfEqualDeadlineTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscEdfMissingDeadlineTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscEdfMissingDeadlineFifoTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrPeekTestCase(),
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrEmptyClassTestCase(),
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrPeekAfterWeightTestCase(),
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrUnderfilledClassTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrMidRoundRefillTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrExpiredPacketTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrPeekExpiredPacketTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscDeadlineMissTraceTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscExpiredDropTraceTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscDefaultDeadlineTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrMinimumWeightsTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrValidWeightsTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscWrrInvalidWeightsTestCase,
                    TestCase::Duration::QUICK);

        AddTestCase(new FutureNetQueueDiscExpiredDropFreesCapacityTestCase,
                    TestCase::Duration::QUICK);
    }

};

static FutureNetQueueDiscTestSuite futurenetQueueDiscTestSuite;