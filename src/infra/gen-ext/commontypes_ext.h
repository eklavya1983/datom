#pragma once
#include <ostream>

namespace infra {
struct ServiceInfo;
struct RingInfo;
struct DataSphereInfo;
struct VolumeInfo;

std::ostream& operator << (std::ostream& out, const ServiceInfo &info);
std::ostream& operator << (std::ostream& out, const RingInfo &info);
std::ostream& operator << (std::ostream& out, const DataSphereInfo &info);
std::ostream& operator << (std::ostream& out, const VolumeInfo &info);

bool isRingMember(const RingInfo& info, const std::string serviceId);

}  // namespace infra
