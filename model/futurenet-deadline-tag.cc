#include "futurenet-deadline-tag.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DeadlineTag");

NS_OBJECT_ENSURE_REGISTERED(DeadlineTag);

DeadlineTag::DeadlineTag()
    : m_deadline(Time::Min())
{
}

TypeId
DeadlineTag::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DeadlineTag")
            .SetParent<Tag>()
            .SetGroupName("FutureNet")
            .AddConstructor<DeadlineTag>();

    return tid;
}

TypeId
DeadlineTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
DeadlineTag::GetSerializedSize() const
{
    return 8;
}

void
DeadlineTag::Serialize(TagBuffer i) const
{
    i.WriteU64(m_deadline.GetNanoSeconds());
}

void
DeadlineTag::Deserialize(TagBuffer i)
{
    m_deadline = NanoSeconds(i.ReadU64());
}

void
DeadlineTag::Print(std::ostream& os) const
{
    os << "deadline=" << m_deadline;
}

void
DeadlineTag::SetDeadline(Time deadline)
{
    m_deadline = deadline;
}

Time
DeadlineTag::GetDeadline() const
{
    return m_deadline;
}

} // namespace ns3
