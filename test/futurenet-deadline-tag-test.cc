#include "ns3/futurenet-deadline-tag.h"
#include "ns3/packet.h"
#include "ns3/test.h"

using namespace ns3;

class FutureNetDeadlineTagTest : public TestCase
{
public:
    FutureNetDeadlineTagTest()
        : TestCase("Test DeadlineTag serialization and copy")
    {
    }

private:
    void DoRun() override
    {
        Ptr<Packet> packet = Create<Packet>(100);

        DeadlineTag tag;
        tag.SetDeadline(MilliSeconds(50));

        packet->AddPacketTag(tag);

        DeadlineTag receivedTag;

        bool found = packet->PeekPacketTag(receivedTag);

        NS_TEST_ASSERT_MSG_EQ(found,
                              true,
                              "DeadlineTag was not found");

        NS_TEST_ASSERT_MSG_EQ(receivedTag.GetDeadline(),
                              MilliSeconds(50),
                              "Deadline was not preserved");
    }
};

class FutureNetDeadlineTagTestSuite : public TestSuite
{
public:
    FutureNetDeadlineTagTestSuite()
        : TestSuite("futurenet-deadline-tag",
                    Type::UNIT)
    {
        AddTestCase(new FutureNetDeadlineTagTest,
                    TestCase::Duration::QUICK);
    }
};

static FutureNetDeadlineTagTestSuite
    futurenetDeadlineTagTestSuite;
