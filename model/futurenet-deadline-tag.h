#ifndef FUTURENET_DEADLINE_TAG_H
#define FUTURENET_DEADLINE_TAG_H

#include "ns3/tag.h"
#include "ns3/nstime.h"

namespace ns3
{

class DeadlineTag : public Tag
{
public:
    DeadlineTag();

    static TypeId GetTypeId();

    TypeId GetInstanceTypeId() const override;

    uint32_t GetSerializedSize() const override;

    void Serialize(TagBuffer i) const override;

    void Deserialize(TagBuffer i) override;

    void Print(std::ostream& os) const override;

    void SetDeadline(Time deadline);

    Time GetDeadline() const;

private:
    Time m_deadline;
};

} // namespace ns3

#endif // FUTURENET_DEADLINE_TAG_H
