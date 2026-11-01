#pragma once

#include <iostream>
#include <filesystem>
#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#include <vector>
#include <unordered_map>
#include <optional>
#include <unordered_set>
#include "flatbuffers/flatbuffers.h"
#include "fbs-gen-cpp/mesh-asset_generated.h"
#include "fbs-gen-cpp/image-asset_generated.h"
#include "fbs-gen-cpp/prefab-asset_generated.h"
#include "fbs-gen-cpp/scene-asset_generated.h"
#include "fbs-gen-cpp/project-setting-asset_generated.h"

namespace core{
  class GpuVBO{
    private:
      SDL_GPUDevice *_device;
      SDL_GPUBuffer *_vertexVBO = nullptr;
      SDL_GPUBuffer *_indexVBO = nullptr;
      Uint32 _numIndices;
      bool _inited = false;
    public:
      ~GpuVBO();
      GpuVBO(SDL_GPUDevice *device);
      SDL_GPUBuffer* getVertexVBO();
      SDL_GPUBuffer* getIndexVBO();
      Uint32 getNumIndices();
      bool init(const FBSAsset::Primitive *primitive);
  };
}
