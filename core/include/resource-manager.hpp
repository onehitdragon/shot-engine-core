#pragma once

#include "gpu-resource.hpp"

namespace fs = std::filesystem;

namespace core{
  enum class AssetType{
    other, image, mesh, prefab, scene, hdr, setting, font, text
  };

  struct PAKEntry{
    std::string uuid;
    AssetType type;
    uint32_t offset;
    uint32_t size;
  };

  enum class PAKQueueState{
    fail, pending, success
  };

  struct PAKQueueData{
    std::vector<char> buffer;
    std::string uuid;
  };

  class PAKFile{
    private:
      bool _inited = false;
      std::string _id;
      std::unordered_map<std::string, PAKEntry> _pakEntryMap;
      SDL_AsyncIO *_asyncIO = nullptr;
      SDL_AsyncIOQueue *_asyncIOQueue = nullptr;
      std::unordered_map<std::string, PAKQueueData> _requestedMap;
      std::unordered_map<std::string, PAKQueueData> _completedMap;
    public:
      ~PAKFile();
      PAKFile() = default;
      bool init(const std::string &pakFileName);
      void info();
      bool getUuidByType(const AssetType &type, std::string &uuid);
      PAKQueueState getFileEntry(
        const std::string &uuid,
        PAKEntry &entryOut,
        std::vector<char> &out
      );
  };

  enum class ResourceState{
    missing, pending, stable
  };

  struct Resource{
    ResourceState state;
    PAKEntry pakEntry;
    std::vector<char> raw;
    const void* asset = nullptr;
    std::vector<GpuVBO*> gpuVBOs;
    int refCount = 0;
  };

  class ResourceManager{
    private:
      ResourceManager() = default;
      ~ResourceManager() = default;
      bool _inited = false;
      PAKFile *_pakFile = nullptr;
      std::unordered_map<std::string, Resource> _resourceMap;
      SDL_GPUDevice *_device;
      Resource* _getResource(const std::string &uuid);
    public:
      ResourceManager(const ResourceManager&) = delete;
      ResourceManager& operator=(const ResourceManager&) = delete;
      static ResourceManager& GetInstance();
      PAKFile* getPakFile();
      bool init(const std::string &pakFileName, SDL_GPUDevice *device);
      bool release();
      Resource* getResource(const std::string &uuid);
      bool cleanResource();
  };
}
