#pragma once

#include <iostream>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <variant>
#include <vector>

namespace core{
  struct Transform{
    std::string id;
    glm::vec3 pos;
    glm::quat rot;
    glm::vec3 scale;
  };
  struct Mesh{
    std::string id;
    std::string meshRef;
  };
  
  enum MSAAFlag{
    // 2 bits
    msaaX1F = (0 << 0),
    msaaX2F = (1 << 0),
    msaaX4F = (2 << 0),
    msaaX8F = (3 << 0),
    msaaShift = 0,
  };
  enum DepthFormatFlag{
    // 2 bits
    depthFormat16F = (0 << 2),
    depthFormat24F = (1 << 2),
    depthFormat32F = (2 << 2),
    depthFormatShift = 2,
  };
  enum ShaderFlag{
    // 2 bits
    simpleShaderF = (0 << 4),
    pbrShaderF = (1 << 4),
  };
  enum TransparentFlag{
    // 1 bits
    transNoF = (0 << 6),
    transYesF = (1 << 6),
    transShift = 6,
  };
  enum CullingFlag{
    // 2 bits
    cullingNoneF = (0 << 7),
    cullingBackF = (1 << 7),
    cullingFrontF = (2 << 7),
    cullingShift = 7,
  };
  struct SimpleShading{
    std::string id;
    uint32_t shaderFlag = simpleShaderF;
    uint32_t cullingFlag;
    uint32_t transparentFlag;
    glm::vec4 color;
  };

  struct Camera{
    std::string id;
    float fovy;
  };
  using Component = std::variant<Transform, Mesh, SimpleShading, Camera>;
  struct GameObject{
    std::string id;
    std::string name;
    std::vector<Component> components;
    std::vector<GameObject> childs;
  };
  struct Scene{
    std::string id;
    std::string name;
    std::vector<GameObject> roots;
  };
}
