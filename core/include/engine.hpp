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
  enum class CullingType{
    none, back, front, both
  };
  class CullingTypeHelper{
    public:
      static CullingType fromString(std::string culling);
  };
  struct SimpleShading{
    std::string id;
    CullingType culling;
    bool transparent;
    glm::vec3 color;
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
