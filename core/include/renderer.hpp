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

namespace core{
  enum class ShaderType{
    None, SimpleNoCull, SimpleBackCull, Pbr
  };

  struct SimpleShaderParam{
    ShaderType shaderType;
    GpuVBO *gpuVBO;
    glm::mat4 mvpMat4;
    glm::vec3 color;
  };
  class SimpleShader{
    private:
      bool _inited = false;
      SDL_GPUGraphicsPipeline *_pipelineNoCull = nullptr;
      SDL_GPUGraphicsPipeline *_pipelineBackCull = nullptr;
    public:
      ~SimpleShader();
      bool init();
      bool render(const SimpleShaderParam &param);
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
      ShaderType _curShaderType;

      int _width = -1;
      int _height = -1;
      SDL_GPUTexture *_depthTexture = nullptr;
      SDL_GPUTexture* _tryCreateDepthTexture();
    public:
      Renderer(const Renderer&) = delete;
      Renderer& operator=(const Renderer&) = delete;
      static Renderer& GetInstance();
      SDL_Window* getWindow();
      SDL_GPUDevice* getDevice();
      SDL_GPUCommandBuffer* getCommandBuffer();
      SDL_GPURenderPass* getRenderPass();
      void setCurShaderType(ShaderType shaderType);
      ShaderType getCurShaderType();
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
