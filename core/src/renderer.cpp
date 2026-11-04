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
  Uint32 Renderer::getPrevPipelineFlag(){ return _prevPipelineFlag; }
  void Renderer::setPrevPipelineFlag(const Uint32 &flag){ _prevPipelineFlag = flag; }
  RendererFrameState Renderer::getFrameState(){ return _frameState; }
  void Renderer::setSampleCountSetting(SDL_GPUSampleCount sampleCount){ _setting.sampleCount = sampleCount; }
  void Renderer::setDepthFormatSetting(SDL_GPUTextureFormat depthFormat){ _setting.depthFormat = depthFormat; }
  SimpleShader* Renderer::getSimpleShader(){ return _simpleShader; };
  bool Renderer::canRender(){
    assert(_inited && "require call init");
    return _canRender;
  }
  void Renderer::_initRendererInitState(){
    SDL_GPUTextureFormat texFormat = SDL_GetGPUSwapchainTextureFormat(_device, _window);
    Uint32 i = SDL_GPU_SAMPLECOUNT_1;
    for(; i <= SDL_GPU_SAMPLECOUNT_8; i++){
      SDL_GPUSampleCount sampleCount = static_cast<SDL_GPUSampleCount>(i);
      if(SDL_GPUTextureSupportsSampleCount(_device, texFormat, sampleCount)){
        _initState.supportSampleCounts.emplace(sampleCount);
      }
    }

    i = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    for(; i <= SDL_GPU_TEXTUREFORMAT_D32_FLOAT; i++){
      SDL_GPUTextureFormat depthFormat = static_cast<SDL_GPUTextureFormat>(i);
      if(SDL_GPUTextureSupportsFormat(_device, depthFormat, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)){
        _initState.supportDepthFormats.emplace(depthFormat);
      }
    }
  }
  void Renderer::_checkRendererFrameState(){
    // check window size
    int curWidth, curHeight;
    SDL_GetWindowSizeInPixels(_window, &curWidth, &curHeight);
    if(_frameState.width != curWidth || _frameState.height != curHeight){
      _frameState.width = curWidth;
      _frameState.height = curHeight;
      _frameState.windowSizeChanged = true;
    }
    else{
      _frameState.windowSizeChanged = false;
    }

    // check sample count
    if(
      _frameState.sampleCount != _setting.sampleCount && 
      _initState.supportSampleCounts.contains(_setting.sampleCount)
    ){
      _frameState.sampleCount = _setting.sampleCount;
      _frameState.sampleCountChanged = true;
    }
    else{
      _frameState.sampleCountChanged = false;
    }

    // check depth format
    if(
      _frameState.depthFormat != _setting.depthFormat && 
      _initState.supportDepthFormats.contains(_setting.depthFormat)
    ){
      _frameState.depthFormat = _setting.depthFormat;
      _frameState.depthFormatChanged = true;
    }
    else{
      _frameState.depthFormatChanged = false;
    }
  }
  bool Renderer::_recreateMsaaTexture(){
    // clear old
    SDL_ReleaseGPUTexture(_device, _frameState.msaaTexture);

    // create msaa texture
    SDL_Log("create new msaa texture %d %d x%d", _frameState.width, _frameState.height, 1 << _frameState.sampleCount);
    SDL_GPUTextureFormat texFormat = SDL_GetGPUSwapchainTextureFormat(_device, _window);
    SDL_GPUTextureCreateInfo msaaTextureInfo{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = texFormat,
      .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
      .width = static_cast<Uint32>(_frameState.width),
      .height = static_cast<Uint32>(_frameState.height),
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = _frameState.sampleCount,
    };
    _frameState.msaaTexture = SDL_CreateGPUTexture(_device, &msaaTextureInfo);
    if(_frameState.msaaTexture == nullptr){
      SDL_Log("Error while create msaa texture: SDL_CreateGPUTexture %s", SDL_GetError());
      return false;
    }
    return true;
  }
  bool Renderer::_recreateDepthTexture(){
    // clear old
    SDL_ReleaseGPUTexture(_device, _frameState.depthTexture);

    // create depth texture
    SDL_Log("create new depth texture %d %d %d", _frameState.width, _frameState.height, _frameState.depthFormat);
    SDL_GPUTextureCreateInfo depthTextureInfo{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = _frameState.depthFormat,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
      .width = static_cast<Uint32>(_frameState.width),
      .height = static_cast<Uint32>(_frameState.height),
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = _frameState.sampleCount,
    };
    _frameState.depthTexture = SDL_CreateGPUTexture(_device, &depthTextureInfo);
    if(_frameState.depthTexture == nullptr){
      SDL_Log("Error while create depth texture: SDL_CreateGPUTexture %s", SDL_GetError());
      return false;
    }
    return true;
  }
  bool Renderer::_recreateMsaaDepthTextureOnFrameState(){
    if(_frameState.windowSizeChanged || _frameState.sampleCountChanged){
      if(!_recreateMsaaTexture()) return false;
      if(!_recreateDepthTexture()) return false;
    }
    else if(_frameState.depthFormatChanged){
      if(!_recreateDepthTexture()) return false;
    }
    return true;
  }
  bool Renderer::init(){
    assert(!_inited && "dont call init 2 times");

    // create window + device
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

    // init state
    _initRendererInitState();

    // create shaders
    _simpleShader = new SimpleShader();
    if(!_simpleShader->init()) return false;

    // disable vsync
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
    // clear msaa texture
    SDL_ReleaseGPUTexture(_device, _frameState.msaaTexture);
    // clear depth texture
    SDL_ReleaseGPUTexture(_device, _frameState.depthTexture);
    // clear other
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

    _checkRendererFrameState();
    if(!_recreateMsaaDepthTextureOnFrameState()) return false;

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
        .store_op = SDL_GPU_STOREOP_STORE
      };
      if(_frameState.sampleCount > SDL_GPU_SAMPLECOUNT_1){
        colorTargetInfo.texture = _frameState.msaaTexture,
        colorTargetInfo.store_op = SDL_GPU_STOREOP_RESOLVE;
        colorTargetInfo.resolve_texture = swapchainTexture;
      }
      SDL_GPUDepthStencilTargetInfo depthStencilInfo{
        .texture = _frameState.depthTexture,
        .clear_depth = 1.0f,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE
      };
      _renderPass = SDL_BeginGPURenderPass(
        _commandBuffer,
        &colorTargetInfo,
        1,
        &depthStencilInfo
      );
      _prevPipelineFlag = (1U << 31);
    }
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
    SDL_Log("clear SimpleShader(%d pipelines)", _pipelineMap.size());
    Renderer &renderer = Renderer::GetInstance();
    for(const auto& [key, pipeline]: _pipelineMap){
      SDL_ReleaseGPUGraphicsPipeline(renderer.getDevice(), pipeline);
    }
    _pipelineMap.clear();
  }
  bool SimpleShader::init(){
    assert(!_inited && "dont call init 2 times");

    Renderer &renderer = Renderer::GetInstance();
    SDL_GPUDevice *device = renderer.getDevice();
    SDL_Window *window = renderer.getWindow();

    // prepare flag
    Uint32 shaderFlag = ShaderFlag::simpleShaderF;
    std::array msaaFlags{ msaaX1F, msaaX2F, msaaX4F, msaaX8F};
    std::array depthFormatFlags{ depthFormat16F, depthFormat24F, depthFormat32F};
    std::array transFlags{ transNoF, transYesF };
    std::array cullingFlags{ cullingNoneF, cullingBackF, cullingFrontF };

    // create shader
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

    // vertex description
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

    // color target description
    std::array colorTargetDescriptions{
      SDL_GPUColorTargetDescription{
        .format = SDL_GetGPUSwapchainTextureFormat(device, window),
        .blend_state{
          .enable_blend = false,
        }
      }
    };

    // pipeline description
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
      .multisample_state{
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
      },
      .depth_stencil_state{
        .compare_op = SDL_GPU_COMPAREOP_LESS,
        .enable_depth_test = true,
        .enable_depth_write = true,
        .enable_stencil_test = false
      },
      .target_info = SDL_GPUGraphicsPipelineTargetInfo{
        .color_target_descriptions = colorTargetDescriptions.data(),
        .num_color_targets = colorTargetDescriptions.size(),
        .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        .has_depth_stencil_target = true
      },
    };

    for(const auto &msaaFlag: msaaFlags){
      for(const auto &depthFormatFlag: depthFormatFlags){
        for(const auto &transFlag: transFlags){
          for(const auto &cullingFlag: cullingFlags){
            // transparent
            if(transFlag == transNoF){
              colorTargetDescriptions[0].blend_state = {
                .enable_blend = false
              };
            }
            else{
              colorTargetDescriptions[0].blend_state = {
                .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .color_blend_op = SDL_GPU_BLENDOP_ADD,
                .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                .enable_blend = true
              };
            }
            // culling
            if(cullingFlag == cullingNoneF){
              pipelineCreateInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            }
            else if(cullingFlag == cullingBackF){
              pipelineCreateInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
            }
            else if(cullingFlag == cullingFrontF){
              pipelineCreateInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_FRONT;
            }
            // msaa
            if(msaaFlag == msaaX1F){
              pipelineCreateInfo.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
            }
            else if(msaaFlag == msaaX2F){
              pipelineCreateInfo.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_2;
            }
            else if(msaaFlag == msaaX4F){
              pipelineCreateInfo.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_4;
            }
            else if(msaaFlag == msaaX8F){
              pipelineCreateInfo.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_8;
            }
            // depth format
            if(depthFormatFlag == depthFormat16F){
              pipelineCreateInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
            }
            else if(depthFormatFlag == depthFormat24F){
              pipelineCreateInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
            }
            else if(depthFormatFlag == depthFormat32F){
              pipelineCreateInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            }
            // create pipelines
            SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
            if(pipeline == nullptr){
              SDL_Log("Couldn't create _pipelineNoCull! %s", SDL_GetError());
              SDL_ReleaseGPUShader(device, vertexShader);
              SDL_ReleaseGPUShader(device, fragmentShader);
              return false;
            }
            // add pipeline to map
            Uint32 flagKey = shaderFlag | msaaFlag | transFlag | cullingFlag | depthFormatFlag;
            _pipelineMap[flagKey] = pipeline;
          }
        }
      }
    }

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    _inited = true;
    return true;
  }
  bool SimpleShader::render(const SimpleShaderParam &param){
    assert(_inited && "call init first");

    Renderer &renderer = Renderer::GetInstance();
    if(!renderer.canRender()) return true;

    Uint32 pipelineFlag = param.pipelineFlag 
      | (renderer.getFrameState().sampleCount << msaaShift)
      | ((renderer.getFrameState().depthFormat - SDL_GPU_TEXTUREFORMAT_D16_UNORM) << depthFormatShift);

    if(!_pipelineMap.contains(pipelineFlag)){
      SDL_Log("Error: cant find pipeline with flag %d", param.pipelineFlag);
      return false;
    }

    SDL_GPURenderPass *renderPass = renderer.getRenderPass();
    if(pipelineFlag != renderer.getPrevPipelineFlag()){
      SDL_BindGPUGraphicsPipeline(renderPass, _pipelineMap[pipelineFlag]);
      renderer.setPrevPipelineFlag(param.pipelineFlag);
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
    SDL_PushGPUFragmentUniformData(renderer.getCommandBuffer(), 0, &param.color[0], sizeof(param.color));
    SDL_DrawGPUIndexedPrimitives(renderPass, param.gpuVBO->getNumIndices(), 1, 0, 0, 0);
    return true;
  }

}