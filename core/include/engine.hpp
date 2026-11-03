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
  enum ShaderFlag{
    // 2 bits
    simpleShaderF = (0 << 2),
    pbrShaderF = (1 << 2),
  };
  enum TransparentFlag{
    // 1 bits
    transNoF = (0 << 4),
    transYesF = (1 << 4),
    transShift = 4,
  };
  enum CullingFlag{
    // 2 bits
    cullingNoneF = (0 << 5),
    cullingBackF = (1 << 5),
    cullingFrontF = (2 << 5),
    cullingShift = 5,
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
