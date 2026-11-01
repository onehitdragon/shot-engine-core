#include "gpu-resource.hpp"

namespace core{
  GpuVBO::~GpuVBO(){
    SDL_Log("clear vbo");
    SDL_ReleaseGPUBuffer(_device, _vertexVBO);
    SDL_ReleaseGPUBuffer(_device, _indexVBO);
  }
  GpuVBO::GpuVBO(SDL_GPUDevice *device){
    _device = device;
  }
  SDL_GPUBuffer* GpuVBO::getVertexVBO(){ return _vertexVBO; }
  SDL_GPUBuffer* GpuVBO::getIndexVBO(){ return _indexVBO; }
  Uint32 GpuVBO::getNumIndices(){ return _numIndices; }
  bool GpuVBO::init(const FBSAsset::Primitive *primitive){
    assert(!_inited && "dont call init 2 times");

    // get data
    Uint32 interleaveSize = primitive->attribute()->interleaveArray()->size();
    const Uint8 *interleave = primitive->attribute()->interleaveArray()->data();
    Uint32 indexSize = primitive->indices()->size();
    const Uint8 *indices = primitive->indices()->data();
    _numIndices = indexSize / 4; // 32 byte per index

    // create buffer
    SDL_GPUBufferCreateInfo vertexCreateInfo = {
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = interleaveSize,
    };
    SDL_GPUBufferCreateInfo indexCreateInfo = {
      .usage = SDL_GPU_BUFFERUSAGE_INDEX,
      .size = indexSize,
    };
    _vertexVBO = SDL_CreateGPUBuffer(_device, &vertexCreateInfo);
    if(_vertexVBO == nullptr){
      SDL_Log("Couldn't create vertex buffer: %s", SDL_GetError());
      return false;
    }
    _indexVBO = SDL_CreateGPUBuffer(_device, &indexCreateInfo);
    if(_indexVBO == nullptr){
      SDL_Log("Couldn't create index buffer: %s", SDL_GetError());
      SDL_ReleaseGPUBuffer(_device, _vertexVBO);
      return false;
    }

    // create transfer
    SDL_GPUTransferBufferCreateInfo vertexTransferBufferCreateInfo = SDL_GPUTransferBufferCreateInfo{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = interleaveSize,
    };
    SDL_GPUTransferBuffer* vertexTransferBuffer = SDL_CreateGPUTransferBuffer(
      _device, &vertexTransferBufferCreateInfo
    );
    if(vertexTransferBuffer == nullptr){
      SDL_Log("Couldn't create vertex transfer buffer: %s", SDL_GetError());
      SDL_ReleaseGPUBuffer(_device, _vertexVBO);
      SDL_ReleaseGPUBuffer(_device, _indexVBO);
      return false;
    }
    SDL_GPUTransferBufferCreateInfo indexTransferBufferCreateInfo = SDL_GPUTransferBufferCreateInfo{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = indexSize,
    };
    SDL_GPUTransferBuffer* indexTransferBuffer = SDL_CreateGPUTransferBuffer(
      _device, &indexTransferBufferCreateInfo
    );
    if(indexTransferBuffer == nullptr){
      SDL_Log("Couldn't create index transfer buffer: %s", SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(_device, vertexTransferBuffer);
      SDL_ReleaseGPUBuffer(_device, _vertexVBO);
      SDL_ReleaseGPUBuffer(_device, _indexVBO);
      return false;
    }

    // map
    void *vertexTransferData = SDL_MapGPUTransferBuffer(_device, vertexTransferBuffer, false);
    if(vertexTransferData == nullptr){
      SDL_Log("Couldn't map vertex transfer buffer: %s", SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(_device, vertexTransferBuffer);
      SDL_ReleaseGPUTransferBuffer(_device, indexTransferBuffer);
      SDL_ReleaseGPUBuffer(_device, _vertexVBO);
      SDL_ReleaseGPUBuffer(_device, _indexVBO);
      return false;
    }
    SDL_memcpy(vertexTransferData, interleave, interleaveSize);
    SDL_UnmapGPUTransferBuffer(_device, vertexTransferBuffer);

    void *indexTransferData = SDL_MapGPUTransferBuffer(_device, indexTransferBuffer, false);
    if(indexTransferData == nullptr){
      SDL_Log("Couldn't map index transfer buffer: %s", SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(_device, vertexTransferBuffer);
      SDL_ReleaseGPUTransferBuffer(_device, indexTransferBuffer);
      SDL_ReleaseGPUBuffer(_device, _vertexVBO);
      SDL_ReleaseGPUBuffer(_device, _indexVBO);
      return false;
    }
    SDL_memcpy(indexTransferData, indices, indexSize);
    SDL_UnmapGPUTransferBuffer(_device, indexTransferBuffer);

    // upload
    SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(_device);
    if (uploadCmdBuf == nullptr){
      SDL_Log("Couldn't acquire GPU command buffer: %s", SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(_device, vertexTransferBuffer);
      SDL_ReleaseGPUTransferBuffer(_device, indexTransferBuffer);
      SDL_ReleaseGPUBuffer(_device, _vertexVBO);
      SDL_ReleaseGPUBuffer(_device, _indexVBO);
      return false;
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
    SDL_GPUTransferBufferLocation vertexBufferLocation = SDL_GPUTransferBufferLocation{
      .transfer_buffer = vertexTransferBuffer,
      .offset = 0,
    };
    SDL_GPUBufferRegion vertexBufferRegion = SDL_GPUBufferRegion{
      .buffer = _vertexVBO,
      .offset = 0,
      .size = interleaveSize,
    };
    SDL_GPUTransferBufferLocation indexBufferLocation = SDL_GPUTransferBufferLocation{
      .transfer_buffer = indexTransferBuffer,
      .offset = 0,
    };
    SDL_GPUBufferRegion indexBufferRegion = SDL_GPUBufferRegion{
      .buffer = _indexVBO,
      .offset = 0,
      .size = indexSize,
    };
    SDL_UploadToGPUBuffer(copyPass, &vertexBufferLocation, &vertexBufferRegion, false);
    SDL_UploadToGPUBuffer(copyPass, &indexBufferLocation, &indexBufferRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    if(!SDL_SubmitGPUCommandBuffer(uploadCmdBuf)){
      SDL_Log("Couldn't submit GPU command buffer: %s", SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(_device, vertexTransferBuffer);
      SDL_ReleaseGPUTransferBuffer(_device, indexTransferBuffer);
      SDL_ReleaseGPUBuffer(_device, _vertexVBO);
      SDL_ReleaseGPUBuffer(_device, _indexVBO);
      return false;
    }

    SDL_ReleaseGPUTransferBuffer(_device, vertexTransferBuffer);
    SDL_ReleaseGPUTransferBuffer(_device, indexTransferBuffer);
    return true;
  }
}
