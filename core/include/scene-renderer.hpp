#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include "resource-manager.hpp"
#include "renderer.hpp"
#include "engine.hpp"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace core{
  struct ParentData{
    glm::mat4 modelMat4 = glm::mat4(1.0f);
  };
  struct NodeData{
    Uint64 sortKey;
    Uint32 pipelineFlag;
    glm::mat4 modelMat4 = glm::mat4(1.0f);
    Mesh mesh;
    SimpleShading simpleShading;
  };
  struct RenderData{
    bool haveCamera;
    glm::mat4 viewMat4;
    glm::mat4 projMat4;
    glm::mat4 vpMat4;
    glm::vec3 camPos;
    std::vector<NodeData> _opaqueNodes;
    std::vector<NodeData> _transparentNodes;
  };

  class SceneRenderer{
    private:
      SceneRenderer() = default;
      ~SceneRenderer() = default;
      bool _inited = false;
      SDL_Window *_window;
      std::string _settingAssetUuid;
      std::string _mainSceneUuid; // if empty, find by _settingAssetUuid
      Scene *_scene = nullptr; // can change at run time from script...
      RenderData _renderData;
      bool _sceneNodeLooper(
        const uint32_t &type, const void *sceneNode, GameObject &go, bool &isPending
      );
      bool _mapSceneNodeToGo(const FBSEngine::GameObject *goNode, GameObject &go);
      void _goLooper(const GameObject &go, const ParentData &parentData);
      bool _render();
    public:
      SceneRenderer(const SceneRenderer&) = delete;
      SceneRenderer& operator=(const SceneRenderer&) = delete;
      static SceneRenderer& GetInstance();
      bool init(SDL_Window *window);
      bool release();
      bool ensureSceneExist();
      bool render();
  };
  
}
