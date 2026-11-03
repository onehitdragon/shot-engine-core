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

  struct RendererSetting{
    SDL_GPUSampleCount sampleCount = SDL_GPU_SAMPLECOUNT_1; // 0 -> 3
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

      SimpleShader *_simpleShader = nullptr;
      Uint32 _prevPipelineFlag;

      int _width = -1;
      int _height = -1;
      bool _windowSizeChanged;
      void _checkWindowSizeChanged();

      std::vector<SDL_GPUTexture*> _msaaRenderTextures;
      std::vector<SDL_GPUTexture*> _depthTextures;
      bool _tryCreateMsaaAndDepthTextures();
      void _checkSetting();

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
      RendererSetting getSetting();
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
