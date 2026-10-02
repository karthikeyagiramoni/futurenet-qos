#ifndef FUTURENET_TRAFFIC_TAG_H
#define FUTURENET_TRAFFIC_TAG_H

#include "ns3/tag.h"

namespace ns3
{

class FutureNetTrafficTag : public Tag
{
public:
  FutureNetTrafficTag();

  static TypeId GetTypeId();

  TypeId GetInstanceTypeId() const override;

  uint32_t GetSerializedSize() const override;

  void Serialize(TagBuffer i) const override;

  void Deserialize(TagBuffer i) override;

  void Print(std::ostream& os) const override;

  void SetPriority(uint8_t priority);
  uint8_t GetPriority() const;

  void SetImportance(double importance);
  double GetImportance() const;

  void SetFlowId(uint32_t flowId);
  uint32_t GetFlowId() const;

private:
  uint8_t m_priority;
  double m_importance;
  uint32_t m_flowId;
};

} // namespace ns3

#endif // FUTURENET_TRAFFIC_TAG_H
