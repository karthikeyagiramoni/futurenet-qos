#include "futurenet-traffic-tag.h"

#include "ns3/log.h"

#include "ns3/fatal-error.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FutureNetTrafficTag");

NS_OBJECT_ENSURE_REGISTERED(FutureNetTrafficTag);

FutureNetTrafficTag::FutureNetTrafficTag()
    : m_priority(4),
      m_importance(0.0),
      m_flowId(0)
{
}

TypeId
FutureNetTrafficTag::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::FutureNetTrafficTag")
            .SetParent<Tag>()
            .SetGroupName("FutureNet")
            .AddConstructor<FutureNetTrafficTag>();

    return tid;
}

TypeId
FutureNetTrafficTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
FutureNetTrafficTag::GetSerializedSize() const
{
    return 1 + sizeof(double) + 4;
}

void
FutureNetTrafficTag::Serialize(TagBuffer i) const
{
    i.WriteU8(m_priority);
    i.WriteDouble(m_importance);
    i.WriteU32(m_flowId);
}

void
FutureNetTrafficTag::Deserialize(TagBuffer i)
{
    m_priority = i.ReadU8();
    m_importance = i.ReadDouble();
    m_flowId = i.ReadU32();
}

void
FutureNetTrafficTag::Print(std::ostream& os) const
{
    os << "priority=" << static_cast<uint32_t>(m_priority)
       << ", importance=" << m_importance
       << ", flowId=" << m_flowId;
}

void
FutureNetTrafficTag::SetPriority(uint8_t priority)
{
    NS_ABORT_MSG_IF(priority > 7,
                    "Priority must be in the range 0-7");
    m_priority = priority;
}

uint8_t
FutureNetTrafficTag::GetPriority() const
{
    return m_priority;
}

void
FutureNetTrafficTag::SetImportance(double importance)
{
    NS_ABORT_MSG_IF(!std::isfinite(importance) ||
                        importance < 0.0 ||
                        importance > 1.0,
                    "Importance must be finite and in the range [0, 1]");
    m_importance = importance;
}

double
FutureNetTrafficTag::GetImportance() const
{
    return m_importance;
}

void
FutureNetTrafficTag::SetFlowId(uint32_t flowId)
{
    m_flowId = flowId;
}

uint32_t
FutureNetTrafficTag::GetFlowId() const
{
    return m_flowId;
}

} // namespace ns3
