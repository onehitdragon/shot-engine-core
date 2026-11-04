#include "resource-manager.hpp"

namespace core{
  PAKFile::~PAKFile(){
    SDL_Log("clear pak file");
    SDL_CloseAsyncIO(_asyncIO, false, _asyncIOQueue, nullptr);
    SDL_DestroyAsyncIOQueue(_asyncIOQueue);
  }
  bool PAKFile::init(const std::string &pakFileName){
    assert(!_inited && "dont call init 2 times");

    fs::path fullPath(SDL_GetBasePath());
    fullPath /= pakFileName;

    // check pak file
    if(!fs::is_regular_file(fullPath)){
      SDL_Log("Error: cant find pak file: %s",  + fullPath.string().c_str());
      return false;
    }

    // read pak file
    SDL_Log("reading pak file: %s", fullPath.filename().string().c_str());
    SDL_IOStream *iostream = SDL_IOFromFile(fullPath.string().c_str(), "r");
    if(iostream == nullptr){
      SDL_Log("Error: SDL_IOFromFile %s", SDL_GetError());
      return false;
    }
    std::vector<char> headerIdBuf(4);
    if(SDL_ReadIO(iostream, headerIdBuf.data(), headerIdBuf.size()) == 0){
      SDL_Log("Error while read headerId: SDL_ReadIO %s", SDL_GetError());
      SDL_CloseIO(iostream);
      return false;
    }
    _id = std::string(headerIdBuf.data(), 4);
    uint32_t tableOffset;
    uint32_t tableSize;
    if(!SDL_ReadU32LE(iostream, &tableOffset)){
      SDL_Log("Error while read tableOffset: SDL_ReadU32LE %s", SDL_GetError());
      SDL_CloseIO(iostream);
      return false;
    }
    if(!SDL_ReadU32LE(iostream, &tableSize)){
      SDL_Log("Error while read tableSize: SDL_ReadU32LE %s", SDL_GetError());
      SDL_CloseIO(iostream);
      return false;
    }
    int numEntry = tableSize / 68;
    
    // read entries
    std::vector<char> entryIdBuf(56);
    for(int i = 0; i < numEntry; i++){
      PAKEntry entry;
      if(SDL_ReadIO(iostream, entryIdBuf.data(), entryIdBuf.size()) == 0){
        SDL_Log("Error while read entryId: SDL_ReadIO %s", SDL_GetError());
        SDL_CloseIO(iostream);
        return false;
      }
      entry.uuid = std::string(entryIdBuf.data());
      if(!SDL_ReadU32LE(iostream, reinterpret_cast<uint32_t*>(&entry.type))){
        SDL_Log("Error while read entryType: SDL_ReadU32LE %s", SDL_GetError());
        SDL_CloseIO(iostream);
        return false;
      }
      if(!SDL_ReadU32LE(iostream, &entry.offset)){
        SDL_Log("Error while read entryOffset: SDL_ReadU32LE %s", SDL_GetError());
        SDL_CloseIO(iostream);
        return false;
      }
      if(!SDL_ReadU32LE(iostream, &entry.size)){
        SDL_Log("Error while read entrySize: SDL_ReadU32LE %s", SDL_GetError());
        SDL_CloseIO(iostream);
        return false;
      }
      _pakEntryMap[entry.uuid] = entry;
    }
    SDL_CloseIO(iostream);

    // setup async
    _asyncIOQueue = SDL_CreateAsyncIOQueue();
    if(_asyncIOQueue == nullptr){
      SDL_Log("Error: SDL_CreateAsyncIOQueue %s", SDL_GetError());
      return false;
    }
    _asyncIO = SDL_AsyncIOFromFile(fullPath.string().c_str(), "r");
    if(_asyncIO == nullptr){
      SDL_Log("Error: SDL_AsyncIOFromFile %s", SDL_GetError());
      SDL_DestroyAsyncIOQueue(_asyncIOQueue);
      return false;
    }

    _inited = true;
    return true;
  }
  void PAKFile::info(){
    assert(_inited && "call init first");

    SDL_Log("Pak info: id(%s) entries(%d)", _id.c_str(), _pakEntryMap.size());
    for(const auto& [key, value] : _pakEntryMap){
      SDL_Log("Pak info: entry(%s) type(%d)", key.c_str(), value.type);
    }
  }
  bool PAKFile::getUuidByType(const AssetType &type, std::string &uuid){
    assert(_inited && "call init first");

    for(const auto& [key, value] : _pakEntryMap){
      if(value.type == type){
        uuid = key;
        return true;
      }
    }
    return false;
  }
  /**
   * @param out buffer if success
   */
  PAKQueueState PAKFile::getFileEntry(
    const std::string &uuid,
    PAKEntry &entryOut,
    std::vector<char> &out
  ){
    assert(_inited && "call init first");

    // dont exist in entry table
    const auto item = _pakEntryMap.find(uuid);
    if(item == _pakEntryMap.end()){
      return PAKQueueState::fail;
    }
    const PAKEntry &pakEntry = item->second;

    // first request
    if(_requestedMap.find(uuid) == _requestedMap.end()){
      PAKQueueData &queueData = _requestedMap[uuid];
      queueData.uuid = uuid;
      queueData.buffer.resize(pakEntry.size);
      bool result = SDL_ReadAsyncIO(
        _asyncIO,
        queueData.buffer.data(), pakEntry.offset, pakEntry.size,
        _asyncIOQueue,
        &queueData
      );
      if(!result){
        _requestedMap.erase(uuid);
        SDL_Log("error: SDL_ReadAsyncIO %s", SDL_GetError());
        return PAKQueueState::fail;
      }
      return PAKQueueState::pending;
    }

    // pull all complete task
    SDL_AsyncIOOutcome outcome{};
    while(SDL_GetAsyncIOResult(_asyncIOQueue, &outcome)){
      PAKQueueData *queueData = static_cast<PAKQueueData*>(outcome.userdata);
      if(outcome.result != SDL_AsyncIOResult::SDL_ASYNCIO_COMPLETE){
        _requestedMap.erase(queueData->uuid);
        SDL_Log("error while read data of entry uuid %s", queueData->uuid.c_str());
        continue;
      }
      _completedMap.insert(_requestedMap.extract(queueData->uuid));
    }

    //
    if(_completedMap.find(uuid) == _completedMap.end()){
      return PAKQueueState::pending;
    }
    entryOut = pakEntry;
    out = std::move(_completedMap[uuid].buffer);
    _completedMap.erase(uuid);
    return PAKQueueState::success;
  }

  ResourceManager& ResourceManager::GetInstance(){
    static ResourceManager instance;
    return instance;
  }
  PAKFile* ResourceManager::getPakFile(){
    assert(_inited && "require call init");
    return _pakFile;
  }
  bool ResourceManager::init(const std::string &pakFileName, SDL_GPUDevice *device){
    assert(!_inited && "dont call init 2 times");

    _pakFile = new PAKFile();
    if(!_pakFile->init(pakFileName)){
      SDL_Log("Error: _pakFile.init");
      return false;
    }
    _pakFile->info();
    
    _device = device;

    _inited = true;
    return true;
  }
  bool ResourceManager::release(){
    SDL_Log("clear resource manager");
    delete _pakFile;
    for(auto& [key, value]: _resourceMap){
      value.refCount = 0;
    }
    cleanResource();
    return true;
  }
  Resource* ResourceManager::_getResource(const std::string &uuid){
    assert(_inited && "require call init");

    // create if not exist
    const auto item = _resourceMap.find(uuid);
    if(item == _resourceMap.end()){
      Resource &resource = _resourceMap[uuid];
      resource.state = ResourceState::pending;
      return &resource;
    }

    // pending
    Resource &resource = item->second;
    if(resource.state == ResourceState::pending){
      auto status = _pakFile->getFileEntry(uuid, resource.pakEntry, resource.raw);
      if(status == PAKQueueState::pending) return &resource;
      if(status == PAKQueueState::fail){
        resource.state = ResourceState::missing;
        return &resource;
      }
      // complete
      resource.state = ResourceState::stable;
      if(resource.pakEntry.type == AssetType::mesh){
        const auto *meshAsset = FBSAsset::GetMeshAsset(resource.raw.data());
        resource.asset = meshAsset;
        const auto *primitives = meshAsset->primitives();
        for(int i = 0; i < primitives->size(); i++){
          const auto *primitive = primitives->Get(i);
          GpuVBO *gpuVBO = new GpuVBO(_device);
          gpuVBO->init(primitive);
          resource.gpuVBOs.push_back(gpuVBO);
        }
      }
      if(resource.pakEntry.type == AssetType::image){
        resource.asset = FBSAsset::GetImageAsset(resource.raw.data());
      }
      if(resource.pakEntry.type == AssetType::scene){
        resource.asset = FBSAsset::GetSceneAsset(resource.raw.data());
      }
      if(resource.pakEntry.type == AssetType::prefab){
        resource.asset = FBSAsset::GetPrefabAsset(resource.raw.data());
      }
      if(resource.pakEntry.type == AssetType::setting){
        resource.asset = FBSAsset::GetProjectSettingAsset(resource.raw.data());
      }
      return &resource;
    }

    // stable
    if(resource.state == ResourceState::stable){
      return &resource;
    }

    // missing
    return &resource;
  }
  Resource* ResourceManager::getResource(const std::string &uuid){
    Resource *res = _getResource(uuid);
    res->refCount += 1;
    return res;
  }
  bool ResourceManager::cleanResource(){
    std::vector<std::string> keysToRemove;
    for(auto& [key, value]: _resourceMap){
      if(value.refCount == 0){
        keysToRemove.push_back(key);
      }
      else{
        value.refCount = 0;
      }
    }
    for(const std::string &key: keysToRemove){
      Resource &resource = _resourceMap[key];
      for(const GpuVBO *gpuVBO: resource.gpuVBOs){
        delete gpuVBO;
      }
      _resourceMap.erase(key);
      SDL_Log("removed resource %s", key.c_str());
    }
    return true;
  }

}