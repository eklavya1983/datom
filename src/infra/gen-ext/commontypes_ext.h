#pragma once
#include <ostream>

namespace infra {
class ServiceInfo;
class RingInfo;
class DataSphereInfo;
class VolumeInfo;

std::ostream& operator << (std::ostream& out, const ServiceInfo &info);
std::ostream& operator << (std::ostream& out, const RingInfo &info);
std::ostream& operator << (std::ostream& out, const DataSphereInfo &info);
std::ostream& operator << (std::ostream& out, const VolumeInfo &info);

bool isRingMember(const RingInfo& info, const std::string serviceId);

}  // namespace infra
