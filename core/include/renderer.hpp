#pragma once

#include <iostream>
#include <filesystem>
#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#include <vector>
#include <array>
#include <cassert>
#include "gpu-resource.hpp"
#include "glm/mat4x4.hpp"
#include "engine.hpp"

namespace core{
  struct SimpleShaderParam{
    Uint32 pipelineFlag;
    GpuVBO *gpuVBO;
    glm::mat4 mvpMat4;
    glm::vec4 color;
  };
  class SimpleShader{
    private:
      bool _inited = false;
      std::unordered_map<Uint32, SDL_GPUGraphicsPipeline*> _pipelineMap;
    public:
      ~SimpleShader();
      bool init();
      bool render(const SimpleShaderParam &param);
  };

  struct RendererInitState{
    std::unordered_set<SDL_GPUSampleCount> supportSampleCounts;
    std::unordered_set<SDL_GPUTextureFormat> supportDepthFormats;
  };
  struct RendererFrameState{
    int width = -1;
    int height = -1;
    bool windowSizeChanged;
    SDL_GPUSampleCount sampleCount = SDL_GPU_SAMPLECOUNT_1;
    bool sampleCountChanged;
    SDL_GPUTexture *msaaTexture = nullptr;
    SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    bool depthFormatChanged;
    SDL_GPUTexture *depthTexture = nullptr;
  };
  struct RendererSetting{
    SDL_GPUSampleCount sampleCount = SDL_GPU_SAMPLECOUNT_4; // 0 -> 3
    SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM; // 58 -> 60
  };
  class Renderer{
    private:
      Renderer() = default;
      ~Renderer() = default;
      bool _inited = false;
      SDL_Window *_window = nullptr;
      SDL_GPUDevice *_device = nullptr;
      SDL_GPUCommandBuffer *_commandBuffer = nullptr;
      SDL_GPURenderPass *_renderPass = nullptr;
      bool _canRender = false;

      RendererInitState _initState;
      RendererFrameState _frameState;
      void _initRendererInitState();
      void _checkRendererFrameState();
      bool _recreateMsaaTexture();
      bool _recreateDepthTexture();
      bool _recreateMsaaDepthTextureOnFrameState();

      SimpleShader *_simpleShader = nullptr;
      Uint32 _prevPipelineFlag;

      RendererSetting _setting; // can change at runtime
    public:
      Renderer(const Renderer&) = delete;
      Renderer& operator=(const Renderer&) = delete;
      static Renderer& GetInstance();
      SDL_Window* getWindow();
      SDL_GPUDevice* getDevice();
      SDL_GPUCommandBuffer* getCommandBuffer();
      SDL_GPURenderPass* getRenderPass();
      Uint32 getPrevPipelineFlag();
      void setPrevPipelineFlag(const Uint32 &flag);
      RendererFrameState getFrameState();
      void setSampleCountSetting(SDL_GPUSampleCount sampleCount);
      void setDepthFormatSetting(SDL_GPUTextureFormat depthFormat);
      SimpleShader* getSimpleShader();
      bool canRender();
      bool init();
      bool release();
      bool startCommand();
      bool endCommand();
      bool startRender();
      bool endRender();
      SDL_GPUShader* loadShader(
        const std::string &shaderFileName,
        const Uint32 &samplerCount,
        const Uint32 &uniformBufferCount,
        const Uint32 &storageBufferCount,
        const Uint32 &storageTextureCount
      );
  };
}
