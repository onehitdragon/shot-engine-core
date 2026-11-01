#include "renderer.hpp"

namespace core{
  Renderer& Renderer::GetInstance(){
    static Renderer instance;
    return instance;
  }
  SDL_Window* Renderer::getWindow(){
    assert(_inited && "require call init");
    return _window;
  }
  SDL_GPUDevice* Renderer::getDevice(){
    assert(_inited && "require call init");
    return _device;
  }
  SDL_GPUCommandBuffer* Renderer::getCommandBuffer(){
    assert(_inited && "require call init");
    assert(_commandBuffer != nullptr && "require startCommand first");
    return _commandBuffer;
  }
  SDL_GPURenderPass* Renderer::getRenderPass(){
    assert(_inited && "require call init");
    assert(_commandBuffer != nullptr && "require startCommand first");
    assert(_canRender && "check canRender before call getRenderPass");
    return _renderPass;
  }
  void Renderer::setCurShaderType(ShaderType shaderType){ _curShaderType = shaderType; }
  ShaderType Renderer::getCurShaderType(){ return _curShaderType; }
  SimpleShader* Renderer::getSimpleShader(){ return _simpleShader; };
  bool Renderer::canRender(){
    assert(_inited && "require call init");
    return _canRender;
  }
  bool Renderer::init(){
    assert(!_inited && "dont call init 2 times");

    _window = SDL_CreateWindow("hello world", 800, 600, 0);
    if(_window == nullptr){
      SDL_Log("SDL_CreateWindowAndRenderer Error: %s", SDL_GetError());
      return false;
    }

    SDL_GPUShaderFormat formatFlags = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;
    _device = SDL_CreateGPUDevice(formatFlags, true, "vulkan");
    if(_device == nullptr){
      SDL_Log("SDL_CreateGPUDevice Error: %s", SDL_GetError());
      return false;
    }

    if(!SDL_ClaimWindowForGPUDevice(_device, _window)){
      SDL_Log("SDL_ClaimWindowForGPUDevice Error: %s", SDL_GetError());
      return false;
    }

    _simpleShader = new SimpleShader();
    if(!_simpleShader->init()) return false;

    // if(SDL_WindowSupportsGPUPresentMode(_device, _window, SDL_GPU_PRESENTMODE_IMMEDIATE)){
    //   SDL_Log("Can turned off vsync");
    //   if(!SDL_SetGPUSwapchainParameters(
    //     _device, _window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE
    //   )){
    //     SDL_Log("SDL_SetGPUSwapchainParameters Error: %s", SDL_GetError());
    //     return false;
    //   }
    // }

    _inited = true;
    return true;
  }
  bool Renderer::release(){
    SDL_Log("clear renderer");
    delete _simpleShader;
    SDL_ReleaseWindowFromGPUDevice(_device, _window);
    SDL_DestroyWindow(_window);
    SDL_DestroyGPUDevice(_device);
    return true;
  }
  bool Renderer::startCommand(){
    assert(_inited && "require call init");
    assert(_commandBuffer == nullptr && "twice startCommand");

    _commandBuffer = SDL_AcquireGPUCommandBuffer(_device);
    if(_commandBuffer == nullptr){
      SDL_Log("SDL_AcquireGPUCommandBuffer Error: %s", SDL_GetError());
      return false;
    }
    return true;
  }
  bool Renderer::endCommand(){
    assert(_inited && "require call init");
    assert(_commandBuffer != nullptr && "require startCommand first");

    if(!SDL_SubmitGPUCommandBuffer(_commandBuffer)){
      SDL_Log("Couldn't submit GPU command buffer: %s", SDL_GetError());
      return false;
    }
    _commandBuffer = nullptr;
    return true;
  }
  bool Renderer::startRender(){
    assert(_inited && "require call init");
    assert(_commandBuffer != nullptr && "require startCommand first");
    assert(_renderPass == nullptr && "startRender twice");

    SDL_GPUTexture *swapchainTexture;
    if(!SDL_WaitAndAcquireGPUSwapchainTexture(_commandBuffer, _window, &swapchainTexture, nullptr, nullptr)){
      SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture Error: %s", SDL_GetError());
      return false;
    }
    _canRender = swapchainTexture != nullptr;
    if(_canRender){
      SDL_GPUColorTargetInfo colorTargetInfo{
        .texture = swapchainTexture,
        .clear_color = SDL_FColor{0.2, 0.2, 0.2, 1.0},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
      };
      _renderPass = SDL_BeginGPURenderPass(
        _commandBuffer, &colorTargetInfo, 1, nullptr
      );
    }
    _curShaderType = ShaderType::None;
    return true;
  }
  bool Renderer::endRender(){
    assert(_inited && "require call init");
    assert(_commandBuffer != nullptr && "require startCommand first");

    if(_canRender){
      SDL_EndGPURenderPass(_renderPass);
    }
    _canRender = false;
    _renderPass = nullptr;
    return true;
  }
  SDL_GPUShader* Renderer::loadShader(
    const std::string &shaderFileName,
    const Uint32 &samplerCount,
    const Uint32 &uniformBufferCount,
    const Uint32 &storageBufferCount,
    const Uint32 &storageTextureCount
  ){
    assert(_inited && "require call init");

    SDL_GPUShaderStage stage;
    if(shaderFileName.find(".vert") != std::string::npos){
      stage = SDL_GPU_SHADERSTAGE_VERTEX;
    }
    else if(shaderFileName.find(".frag") != std::string::npos){
      stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    else{
      SDL_Log("Couldn't deduce shader stage from file name: %s", shaderFileName.c_str());
      return nullptr;
    }

    std::filesystem::path fullPath(SDL_GetBasePath());
    fullPath /= "shader-gen";
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    SDL_GPUShaderFormat backendFormat = SDL_GetGPUShaderFormats(_device);
    if(backendFormat & SDL_GPU_SHADERFORMAT_SPIRV){
      fullPath /= shaderFileName + ".spv";
      format = SDL_GPU_SHADERFORMAT_SPIRV;
    }
    else if(backendFormat & SDL_GPU_SHADERFORMAT_DXIL){
      fullPath /= shaderFileName + ".dxil";
      format = SDL_GPU_SHADERFORMAT_DXIL;
    }
    else if(backendFormat & SDL_GPU_SHADERFORMAT_MSL){
      fullPath /= shaderFileName + ".msl";
      format = SDL_GPU_SHADERFORMAT_MSL;
    }
    else{
      SDL_Log("Couldn't find a supported shader format for backend %s!", SDL_GetGPUDeviceDriver(_device));
		  return nullptr;
    }

    size_t datasize;
    void *code = SDL_LoadFile(fullPath.string().c_str(), &datasize);
    if(code == nullptr){
      SDL_Log("Couldn't load shader file from disk!\n\t%s", SDL_GetError());
      return nullptr;
    }
    SDL_GPUShaderCreateInfo shaderInfo{
      .code_size = datasize,
      .code = static_cast<uint8_t*>(code),
      .entrypoint = "main",
      .format = format,
      .stage = stage,
      .num_samplers = samplerCount,
      .num_storage_textures = storageTextureCount,
      .num_storage_buffers = storageBufferCount,
      .num_uniform_buffers = uniformBufferCount,
    };
    SDL_GPUShader *shader = SDL_CreateGPUShader(_device, &shaderInfo);
    if(shader == nullptr){
      SDL_Log("Couldn't create shader from file %s: %s", fullPath.c_str(), SDL_GetError());
    }
    SDL_free(code);
    return shader;
  }

  SimpleShader::~SimpleShader(){
    SDL_Log("clear SimpleShader");
    Renderer &renderer = Renderer::GetInstance();
    SDL_GPUDevice *device = renderer.getDevice();
    SDL_ReleaseGPUGraphicsPipeline(device, _pipelineNoCull);
    SDL_ReleaseGPUGraphicsPipeline(device, _pipelineBackCull);
  }
  bool SimpleShader::init(){
    assert(!_inited && "dont call init 2 times");

    Renderer &renderer = Renderer::GetInstance();
    SDL_GPUDevice *device = renderer.getDevice();
    SDL_Window *window = renderer.getWindow();

    SDL_GPUShader* vertexShader = renderer.loadShader("simple-shader/shader.vert", 0, 1, 0, 0);
    if(vertexShader == nullptr){
      SDL_Log("Couldn't create vertex shader!");
      return false;
    }
    SDL_GPUShader* fragmentShader = renderer.loadShader("simple-shader/shader.frag", 0, 1, 0, 0);
    if(fragmentShader == nullptr){
      SDL_Log("Couldn't create fragment shader!");
      SDL_ReleaseGPUShader(device, vertexShader);
      return false;
    }

    std::array vertexBufferDescriptions{
      SDL_GPUVertexBufferDescription{
        .slot = 0,
        .pitch = (3 + 3 + 2 + 3) * 4, // (3 verter, 3 normal, 2 uv, 3 tangent) * floatSize = 4,
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0,
      }
    };
    std::array vertexAttributes{
      SDL_GPUVertexAttribute{
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = 0
      },
    };
    std::array colorTargetDescriptions{
      SDL_GPUColorTargetDescription{
        .format = SDL_GetGPUSwapchainTextureFormat(device, window)
      }
    };
    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{
      .vertex_shader = vertexShader,
      .fragment_shader = fragmentShader,
      .vertex_input_state = SDL_GPUVertexInputState{
        .vertex_buffer_descriptions = vertexBufferDescriptions.data(),
        .num_vertex_buffers = vertexBufferDescriptions.size(),
        .vertex_attributes = vertexAttributes.data(),
        .num_vertex_attributes = vertexAttributes.size()
      },
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state = SDL_GPURasterizerState{
        .fill_mode = SDL_GPU_FILLMODE_FILL,
        .cull_mode = SDL_GPU_CULLMODE_NONE,
        .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
      },
      .target_info = SDL_GPUGraphicsPipelineTargetInfo{
        .color_target_descriptions = colorTargetDescriptions.data(),
        .num_color_targets = colorTargetDescriptions.size(),
      },
    };
    _pipelineNoCull = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
    
    if (_pipelineNoCull == nullptr){
      SDL_Log("Couldn't create _pipelineNoCull! %s", SDL_GetError());
      SDL_ReleaseGPUShader(device, vertexShader);
      SDL_ReleaseGPUShader(device, fragmentShader);
      return false;
    }

    pipelineCreateInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    _pipelineBackCull = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
    if (_pipelineBackCull == nullptr){
      SDL_Log("Couldn't create _pipelineBackCull! %s", SDL_GetError());
      SDL_ReleaseGPUGraphicsPipeline(device, _pipelineNoCull);
      SDL_ReleaseGPUShader(device, vertexShader);
      SDL_ReleaseGPUShader(device, fragmentShader);
      return false;
    }

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    _inited = true;
    return true;
  }
  bool SimpleShader::render(const SimpleShaderParam &param){
    assert(_inited && "call init first");
    ShaderType shaderType = param.shaderType;
    assert(shaderType != ShaderType::None && "bad shaderType(None)");

    Renderer &renderer = Renderer::GetInstance();
    if(!renderer.canRender()) return true;

    SDL_GPURenderPass *renderPass = renderer.getRenderPass();
    ShaderType curShaderType = renderer.getCurShaderType();
    if(shaderType != curShaderType){
      if(shaderType == ShaderType::SimpleNoCull){
        SDL_BindGPUGraphicsPipeline(renderPass, _pipelineNoCull);
      }
      else if(shaderType == ShaderType::SimpleBackCull){
        SDL_BindGPUGraphicsPipeline(renderPass, _pipelineBackCull);
      }
      else{
        SDL_Log("bad shader type");
        return false;
      }
      renderer.setCurShaderType(shaderType);
    }

    std::array vertexBuffers{
			SDL_GPUBufferBinding{
				.buffer = param.gpuVBO->getVertexVBO(),
				.offset = 0,
			},
		};
    std::array indexBuffers{
			SDL_GPUBufferBinding{
				.buffer = param.gpuVBO->getIndexVBO(),
				.offset = 0,
			},
		};
    SDL_BindGPUVertexBuffers(renderPass, 0, vertexBuffers.data(), vertexBuffers.size());
    SDL_BindGPUIndexBuffer(renderPass, indexBuffers.data(), SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_PushGPUVertexUniformData(renderer.getCommandBuffer(), 0, &param.mvpMat4[0][0], sizeof(param.mvpMat4));
    glm::vec3 color(1.0f);
    SDL_PushGPUFragmentUniformData(renderer.getCommandBuffer(), 0, &color[0], sizeof(color));
    SDL_DrawGPUIndexedPrimitives(renderPass, param.gpuVBO->getNumIndices(), 1, 0, 0, 0);
    return true;
  }

}