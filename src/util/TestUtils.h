#pragma once
namespace util {
/**
* @brief Derive this class to automatically get read/write prototype used in thrift
* based serialization
*/
struct DummySerializable {
  template <class Protocol_>
  uint32_t read(Protocol_* iprot) {return 0;}
  template <class Protocol_>
  uint32_t serializedSize(Protocol_* prot_) const {return 0;}
  template <class Protocol_>
  uint32_t serializedSizeZC(Protocol_* prot_) const {return 0;}
  template <class Protocol_>
  uint32_t write(Protocol_* prot_) const {return 0;}
    
};
}  // namespace util
