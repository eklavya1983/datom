#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/typestr.h>

template <>
const char* typeStr<infra::PingMsg>()
{
    return "PingMsg";
}
template <>
const char* typeStr<infra::PingRespMsg>()
{
    return "PingRespMsg";
}

namespace infra {

std::ostream& operator << (std::ostream& out, const ServiceInfo &info)
{
    out << " serviceinfo:[" << info.dataSphereId << ":" << info.id << "]"
        << " ip:" << info.ip << " port:" << info.port;
    return out;
}

std::ostream& operator << (std::ostream& out, const RingInfo &info)
{
    out << " [ringinfo id:" << info.id << " members:";
    for (const auto &i : info.memberIds) {
        out << i << " ";
    }
    out << "]";
    return out;
}

std::ostream& operator << (std::ostream& out, const DataSphereInfo &info)
{
    out << " [datasphere id:" << info.id << "]";
    return out;
}

std::ostream& operator << (std::ostream& out, const VolumeInfo &info)
{
    out << " [volume id:" << info.id << "]";
    return out;
}

bool isRingMember(const RingInfo& info, const std::string serviceId)
{
    return (std::find(info.memberIds.begin(),
                      info.memberIds.end(),
                      serviceId) != info.memberIds.end());
}

int64_t getId(const VolumeInfo& info)
{
    return info.id;
}
}  // namespace infra
