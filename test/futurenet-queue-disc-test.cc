#include "ns3/futurenet-queue-disc.h"
#include "ns3/test.h"

using namespace ns3;

class FutureNetQueueDiscTestCase : public TestCase
{
public:
    FutureNetQueueDiscTestCase()
        : TestCase("Test FutureNetQueueDisc construction and internal queue")
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
                              "Internal queue should not exist before initialization");

        queueDisc->Initialize();

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetNInternalQueues(),
                              1,
                              "FutureNetQueueDisc should create one internal queue");

        NS_TEST_ASSERT_MSG_EQ(queueDisc->GetDeadlineMissCount(),
                              0,
                              "Initial deadline miss count should be zero");
    }
};

class FutureNetQueueDiscTestSuite : public TestSuite
{
public:
    FutureNetQueueDiscTestSuite()
        : TestSuite("futurenet-queue-disc", TestSuite::Type::UNIT)
    {
        AddTestCase(new FutureNetQueueDiscTestCase,
                    TestCase::Duration::QUICK);
    }
};

static FutureNetQueueDiscTestSuite futurenetQueueDiscTestSuite;
