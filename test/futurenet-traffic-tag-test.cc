#include "ns3/futurenet-traffic-tag.h"
#include "ns3/packet.h"
#include "ns3/test.h"

using namespace ns3;

class FutureNetTrafficTagTest : public TestCase
{
public:
    FutureNetTrafficTagTest()
        : TestCase("Test FutureNetTrafficTag serialization and copy")
    {
    }

private:
    void DoRun() override
    {
        Ptr<Packet> packet = Create<Packet>(100);

        FutureNetTrafficTag tag;
        tag.SetPriority(2);
        tag.SetImportance(0.8);
        tag.SetFlowId(10);

        packet->AddPacketTag(tag);

        FutureNetTrafficTag receivedTag;

        bool found = packet->PeekPacketTag(receivedTag);

        NS_TEST_ASSERT_MSG_EQ(found,
                              true,
                              "FutureNetTrafficTag was not found");

        NS_TEST_ASSERT_MSG_EQ(receivedTag.GetPriority(),
                              2,
                              "Priority was not preserved");

        NS_TEST_ASSERT_MSG_EQ_TOL(receivedTag.GetImportance(),
                                   0.8,
                                   0.000001,
                                   "Importance was not preserved");

        NS_TEST_ASSERT_MSG_EQ(receivedTag.GetFlowId(),
                              10,
                              "Flow ID was not preserved");
    }
};

class FutureNetTrafficTagTestSuite : public TestSuite
{
public:
    FutureNetTrafficTagTestSuite()
        : TestSuite("futurenet-traffic-tag",
                    Type::UNIT)
    {
        AddTestCase(new FutureNetTrafficTagTest,
                    TestCase::Duration::QUICK);
    }
};

static FutureNetTrafficTagTestSuite
    futurenetTrafficTagTestSuite;
