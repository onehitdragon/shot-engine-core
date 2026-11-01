#include "engine.hpp"

namespace core{
  CullingType CullingTypeHelper::fromString(std::string culling){
    if(culling == "back") return CullingType::back;
    if(culling == "front") return CullingType::back;
    if(culling == "both") return CullingType::back;
    return CullingType::none;
  }
}
