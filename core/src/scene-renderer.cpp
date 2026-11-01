#include "scene-renderer.hpp"

namespace core{
  SceneRenderer& SceneRenderer::GetInstance(){
    static SceneRenderer instance;
    return instance;
  }
  bool SceneRenderer::release(){
    SDL_Log("clean scene renderer");
    delete _scene;
    return true;
  }
  bool SceneRenderer::init(SDL_Window *window){
    assert(!_inited && "dont call init 2 times");

    ResourceManager &resourceManager = ResourceManager::GetInstance();
    if(!resourceManager.getPakFile()->getUuidByType(AssetType::setting, _settingAssetUuid)){
      SDL_Log("Error: missing project setting asset");
      return false;
    }
    _window = window;

    _inited = true;
    return true;
  }
  bool SceneRenderer::ensureSceneExist(){
    assert(_inited && "call init first");
    if(_scene != nullptr) return true;
    
    // get main scene uuid
    ResourceManager &resourceManager = ResourceManager::GetInstance(); 
    if(_mainSceneUuid.empty()){
      Resource *settingRes = resourceManager.getResource(_settingAssetUuid);
      if(settingRes == nullptr || settingRes->state == ResourceState::missing){
        SDL_Log("missing project setting asset resource");
        return false;
      }
      if(settingRes->state == ResourceState::pending) return true;
      const auto *settingAsset = reinterpret_cast<const FBSAsset::ProjectSettingAsset*>(settingRes->asset);
      if(settingAsset == nullptr){
        SDL_Log("bad settingAsset");
        return false;
      }
      const auto *sceneSetting = settingAsset->scene();
      if(sceneSetting == nullptr){
        SDL_Log("bad sceneSetting");
        return false;
      }
      const auto *mainRef = sceneSetting->mainRef();
      if(mainRef == nullptr || mainRef->empty()){
        SDL_Log("bad mainRef");
        return false;
      }
      _mainSceneUuid = mainRef->str();
      SDL_Log("main scene uuid %s", _mainSceneUuid.c_str());
    }

    // get main scene
    Resource *sceneRes = resourceManager.getResource(_mainSceneUuid);
    if(sceneRes == nullptr || sceneRes->state == ResourceState::missing){
      SDL_Log("cant find scene resource %s", _mainSceneUuid.c_str());
      return false;
    }
    if(sceneRes->state == ResourceState::pending) return true;
    const FBSAsset::SceneAsset *sceneAsset = reinterpret_cast<const FBSAsset::SceneAsset*>(sceneRes->asset);
    if(sceneAsset == nullptr){
      SDL_Log("bad sceneAsset");
      return false;
    }
    const FBSEngine::Scene *scene = sceneAsset->scene();
    if(scene == nullptr){
      SDL_Log("bad scene");
      return false;
    }

    // create _scene
    const auto *rootTypes = scene->roots_type();
    const auto *roots = scene->roots();
    if(rootTypes == nullptr){
      SDL_Log("bad rootTypes");
      return false;
    }
    if(roots == nullptr){
      SDL_Log("bad roots");
      return false;
    }
    std::vector<GameObject> rootGos(rootTypes->size());
    bool isPending = false;
    for(int i = 0; i < rootTypes->size(); i++){
      if(!_sceneNodeLooper(rootTypes->Get(i), roots->Get(i), rootGos[i], isPending)){
        return false;
      }
    }
    if(isPending) return true;
    _scene = new Scene();
    _scene->id = scene->id()->str();
    _scene->name = scene->name()->str();
    _scene->roots = std::move(rootGos);
    SDL_Log("create scene name: %s id: %s", _scene->name.c_str(), _scene->id.c_str());
    return true;
  }
  bool SceneRenderer::_sceneNodeLooper(
    const uint32_t &type, const void *sceneNode, GameObject &go, bool &isPending
  ){
    assert(_inited && "call init first");
    ResourceManager &resourceManager = ResourceManager::GetInstance();

    // get goNode
    const FBSEngine::GameObject *goNode = nullptr;
    if(type == FBSEngine::SceneNode_GameObjectPrefab){
      const auto *prefab = reinterpret_cast<const FBSEngine::GameObjectPrefab*>(sceneNode);
      std::string prefabUuid = prefab->prefab_ref()->str();
      Resource *prefabRes = resourceManager.getResource(prefabUuid);
      if(prefabRes == nullptr || prefabRes->state == ResourceState::missing){
        SDL_Log("missing prefab %s", prefabUuid.c_str());
        return false;
      }
      if(prefabRes->state == ResourceState::pending){
        isPending = true;
        return true;
      }
      const auto *prefabAsset = reinterpret_cast<const FBSAsset::PrefabAsset*>(prefabRes->asset);
      if(prefabAsset == nullptr){
        SDL_Log("bad prefab asset %s", prefabUuid.c_str());
        return false;
      }
      goNode = prefabAsset->root();
    }
    else if(type == FBSEngine::SceneNode_GameObject){
      goNode = reinterpret_cast<const FBSEngine::GameObject*>(sceneNode);
    }
    else{
      SDL_Log("bad node type");
      return false;
    }

    // map goNode -> go
    if(goNode == nullptr){
      SDL_Log("bad goNode");
      return false;
    }
    if(!_mapSceneNodeToGo(goNode, go)) return false;

    // render childs
    const auto *childTypes = goNode->childs_type();
    const auto *childs = goNode->childs();
    if(childTypes == nullptr){
      SDL_Log("bad childTypes");
      return false;
    }
    if(childs == nullptr){
      SDL_Log("bad childs");
      return false;
    }
    go.childs.resize(childTypes->size());
    for(int i = 0; i < childTypes->size(); i++){
      if(!_sceneNodeLooper(childTypes->Get(i), childs->Get(i), go.childs[i], isPending)){
        return false;
      }
    }
    return true;
  }
  bool SceneRenderer::_mapSceneNodeToGo(const FBSEngine::GameObject *goNode, GameObject &go){
    const auto *componentTypes = goNode->components_type();
    const auto *components = goNode->components();
    if(componentTypes == nullptr){
      SDL_Log("bad componentTypes");
      return false;
    }
    if(components == nullptr){
      SDL_Log("bad components");
      return false;
    }
    const FBSEngine::Transform *transformComponent = nullptr;
    const FBSEngine::Mesh *meshComponent = nullptr;
    const FBSEngine::SimpleShading *simpleShading = nullptr;
    const FBSEngine::PbrShading *pbrShading = nullptr;
    const FBSEngine::Camera *cameraComponent = nullptr;
    for(int i = 0; i < componentTypes->size(); i++){
      const auto cType = componentTypes->Get(i);
      if(cType == FBSEngine::Component_Transform){
        transformComponent = components->GetAs<FBSEngine::Transform>(i);
        if(transformComponent == nullptr){
          SDL_Log("bad transform component");
          return false;
        }
      }
      else if(cType == FBSEngine::Component_Mesh){
        meshComponent = components->GetAs<FBSEngine::Mesh>(i);
        if(meshComponent == nullptr){
          SDL_Log("bad mesh component");
          return false;
        }
      }
      else if(cType == FBSEngine::Component_SimpleShading){
        simpleShading = components->GetAs<FBSEngine::SimpleShading>(i);
        if(simpleShading == nullptr){
          SDL_Log("bad simpleShading component");
          return false;
        }
      }
      else if(cType == FBSEngine::Component_PbrShading){
        pbrShading = components->GetAs<FBSEngine::PbrShading>(i);
        if(pbrShading == nullptr){
          SDL_Log("bad pbrShading component");
          return false;
        }
      }
      else if(cType == FBSEngine::Component_Camera){
        cameraComponent = components->GetAs<FBSEngine::Camera>(i);
        if(cameraComponent == nullptr){
          SDL_Log("bad camera component");
          return false;
        }
      }
    }
    if(transformComponent == nullptr){
      SDL_Log("bad transformComponent");
      return false;
    }
    const auto *pos = transformComponent->pos();
    if(pos == nullptr){
      SDL_Log("bad transformComponent(pos)");
      return false;
    }
    const auto *rot = transformComponent->rot();
    if(rot == nullptr){
      SDL_Log("bad transformComponent(rot)");
      return false;
    }
    const auto *scale = transformComponent->scale();
    if(scale == nullptr){
      SDL_Log("bad transformComponent(scale)");
      return false;
    }
    go.components.push_back(Transform{
      .id = transformComponent->id()->str(),
      .pos = glm::vec3(pos->x(), pos->y(), pos->z()),
      .rot = glm::quat(rot->w(), rot->x(), rot->y(), rot->z()),
      .scale = glm::vec3(scale->x(), scale->y(), scale->z()),
    });
    if(meshComponent != nullptr){
      go.components.push_back(Mesh{
        .id = transformComponent->id()->str(),
        .meshRef = meshComponent->mesh_ref()->str(),
      });
    }
    if(simpleShading != nullptr){
      const auto *color = simpleShading->color();
      if(color == nullptr){
        SDL_Log("bad simpleShading(color)");
        return false;
      }
      go.components.push_back(SimpleShading{
        .id = simpleShading->id()->str(),
        .culling = static_cast<CullingType>(simpleShading->culling()),
        .transparent = simpleShading->transparent(),
        .color = glm::vec3(color->x(), color->y(), color->z()),
      });
    }
    if(cameraComponent != nullptr){
      go.components.push_back(Camera{
        .id = cameraComponent->id()->str(),
        .fovy = static_cast<float>(cameraComponent->fovy())
      });
    }
    go.id = goNode->id()->str();
    go.name = goNode->name()->str();
    return true;
  }
  bool SceneRenderer::render(){
    assert(_inited && "call init first");
    ResourceManager &resourceManager = ResourceManager::GetInstance();

    // clear
    _renderData.haveCamera = false;
    _renderData._opaqueNodes.clear();
    _renderData._transparentNodes.clear();

    // loop
    if(_scene == nullptr) return true;
    for(const auto &goRoot: _scene->roots){
      _goLooper(goRoot, {});
    }

    // render
    if(!_render()) return false;

    return true;
  }
  void SceneRenderer::_goLooper(const GameObject &go, const ParentData &parentData){
    assert(_inited && "call init first");
    ResourceManager &resourceManager = ResourceManager::GetInstance();

    int cameraIdx = -1;
    int transformIdx = -1;
    int meshIdx = -1;
    int simpleShadingIdx = -1;
    for(int i = 0; i < go.components.size(); i++){
      const auto &component = go.components[i];
      if(std::holds_alternative<Camera>(component)){
        cameraIdx = i;
      }
      else if(std::holds_alternative<Transform>(component)){
        transformIdx = i;
      }
      else if(std::holds_alternative<Mesh>(component)){
        meshIdx = i;
      }
      else if(std::holds_alternative<SimpleShading>(component)){
        simpleShadingIdx = i;
      }
    }
    glm::mat4 modelMat4 = parentData.modelMat4;
    if(transformIdx == -1){
      return;
    }
    else{
      const auto &transformComponent = go.components[transformIdx];
      const auto &transform = std::get<Transform>(transformComponent);
      modelMat4 = glm::translate(modelMat4, transform.pos);
      modelMat4 = modelMat4 * glm::toMat4(transform.rot);
      modelMat4 = glm::scale(modelMat4, transform.scale);
    }
    if(cameraIdx != -1){
      // build projection matrix
      const auto &cameraComponent = go.components[cameraIdx];
      const auto &camera = std::get<Camera>(cameraComponent);
      int width, height;
      SDL_GetWindowSizeInPixels(_window, &width, &height);
      float aspect = static_cast<float>(width) / static_cast<float>(height);
      _renderData.projMat4 = glm::perspectiveRH_ZO(glm::radians(camera.fovy), aspect, 0.1f, 1000.0f);

      // build view matrix
      _renderData.viewMat4 = glm::inverse(modelMat4);

      // build vp matrix
      _renderData.vpMat4 = _renderData.projMat4 * _renderData.viewMat4;

      // build camera pos
      _renderData.camPos = glm::vec3(modelMat4[3]);

      //
      _renderData.haveCamera = true;
    }
    if(meshIdx != -1 && simpleShadingIdx != -1){
      const auto &meshComponent = go.components[meshIdx];
      const auto &mesh = std::get<Mesh>(meshComponent);
      resourceManager.getResource(mesh.meshRef); // get resource ahead
      const auto &simpleShadingComponent = go.components[simpleShadingIdx];
      const auto &simpleShading = std::get<SimpleShading>(simpleShadingComponent);
      if(!simpleShading.transparent){
        auto &nodeData = _renderData._opaqueNodes.emplace_back(NodeData{});
        nodeData.modelMat4 = modelMat4;
        nodeData.mesh = mesh;
        nodeData.simpleShading = simpleShading;
        if(simpleShading.culling == CullingType::back){
          nodeData.shaderType = ShaderType::SimpleBackCull;
        }
        else{
          nodeData.shaderType = ShaderType::SimpleNoCull;
        }
      }
      else{
        auto &nodeData = _renderData._transparentNodes.emplace_back(NodeData{});
        nodeData.modelMat4 = modelMat4;
        nodeData.mesh = mesh;
        nodeData.simpleShading = simpleShading;
        if(simpleShading.culling == CullingType::back){
          nodeData.shaderType = ShaderType::SimpleBackCull;
        }
        else{
          nodeData.shaderType = ShaderType::SimpleNoCull;
        }
      }
    }

    ParentData selftData{
      .modelMat4 = modelMat4
    };
    for(const auto &child: go.childs){
      _goLooper(child, selftData);
    }
  }
  bool SceneRenderer::_render(){
    assert(_inited && "call init first");
    ResourceManager &resourceManager = ResourceManager::GetInstance();
    Renderer &renderer = Renderer::GetInstance();
    
    if(!_renderData.haveCamera) return true;
    // setup sort key
    for(auto &opaqueNode: _renderData._opaqueNodes){
      float distanceToCam = glm::distance(_renderData.camPos, glm::vec3(opaqueNode.modelMat4[3]));
      Uint32 distanceBits = static_cast<Uint32>(distanceToCam * 1000.0f);
      opaqueNode.sortKey = 0;
      opaqueNode.sortKey |= (static_cast<Uint64>(opaqueNode.shaderType) << 32);
      opaqueNode.sortKey |= (static_cast<Uint64>(distanceBits));
    }
    for(auto &transparentNode: _renderData._transparentNodes){
      float distanceToCam = glm::distance(_renderData.camPos, glm::vec3(transparentNode.modelMat4[3]));
      Uint32 distanceBits = ~static_cast<Uint32>(distanceToCam * 1000.0f);
      transparentNode.sortKey = 0;
      transparentNode.sortKey |= (static_cast<Uint64>(distanceBits) << 32);
      transparentNode.sortKey |= (static_cast<Uint64>(transparentNode.shaderType));
    }

    // sort
    std::sort(
      _renderData._opaqueNodes.begin(),
      _renderData._opaqueNodes.end(),
      [](const NodeData &a, const NodeData &b){
        return a.sortKey < b.sortKey;
      }
    );
    std::sort(
      _renderData._transparentNodes.begin(),
      _renderData._transparentNodes.end(),
      [](const NodeData &a, const NodeData &b){
        return a.sortKey < b.sortKey;
      }
    );

    // render opaque
    for(const auto &opaqueNode: _renderData._opaqueNodes){
      Resource *meshRes = resourceManager.getResource(opaqueNode.mesh.meshRef);
      if(meshRes == nullptr || meshRes->state == ResourceState::missing) continue;
      if(meshRes->state == ResourceState::pending) continue;
      const auto &gpuVBO = meshRes->gpuVBOs[0];
      if(
        opaqueNode.shaderType == ShaderType::SimpleBackCull || 
        opaqueNode.shaderType == ShaderType::SimpleNoCull
      ){
        bool result = renderer.getSimpleShader()->render({
          .shaderType = opaqueNode.shaderType,
          .gpuVBO = gpuVBO,
          .mvpMat4 = _renderData.vpMat4 * opaqueNode.modelMat4,
          .color = opaqueNode.simpleShading.color
        });
        if(!result) return false;
      }
    }

    return true;
  }

}
