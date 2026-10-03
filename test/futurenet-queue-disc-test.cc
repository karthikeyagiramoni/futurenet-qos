#include "ns3/futurenet-queue-disc.h"
#include "ns3/futurenet-traffic-tag.h"
#include "ns3/futurenet-deadline-tag.h"
#include "ns3/packet.h"
#include "ns3/test.h"

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

        queueDisc->Initialize();

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNInternalQueues(),
                              4,
                              "FutureNetQueueDisc should create four priority queues");

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
    }

};

static FutureNetQueueDiscTestSuite futurenetQueueDiscTestSuite;