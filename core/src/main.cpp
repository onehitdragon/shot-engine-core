#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include "renderer.hpp"
#include "resource-manager.hpp"
#include "scene-renderer.hpp"
#include <glm/mat4x4.hpp>

struct MyAppState{
    Uint64 lastTime;
    Uint64 second;
    Uint64 numFrame;
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv){
    SDL_Log("hello world!");
    MyAppState *myAppState = new MyAppState();
    *appstate = myAppState;

    // fps
    myAppState->lastTime = SDL_GetTicks();

    // init renderer
    core::Renderer &renderer = core::Renderer::GetInstance();
    if(!renderer.init()){
        SDL_Log("Error: renderer.init");
        return SDL_APP_FAILURE; 
    }

    // init resource manager
    core::ResourceManager &resourceManager = core::ResourceManager::GetInstance();
    if(!resourceManager.init("pak0.pak", renderer.getDevice())){
        SDL_Log("Error: resourceManager.init");
        return SDL_APP_FAILURE;
    }

    // init scene
    core::SceneRenderer &sceneRenderer = core::SceneRenderer::GetInstance();
    if(!sceneRenderer.init(renderer.getWindow())){
        SDL_Log("Error: sceneRenderer.init");
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppIterate(void *appstate){
    MyAppState *myAppState = static_cast<MyAppState*>(appstate);
    core::ResourceManager &resourceManager = core::ResourceManager::GetInstance();
    core::Renderer &renderer = core::Renderer::GetInstance();
    core::SceneRenderer &sceneRenderer = core::SceneRenderer::GetInstance();

    // fps
    Uint64 now = SDL_GetTicks();
    myAppState->second += now - myAppState->lastTime;
    myAppState->lastTime = now;
    myAppState->numFrame++;
    if(myAppState->second >= 1000){
        SDL_SetWindowTitle(renderer.getWindow(), std::to_string(myAppState->numFrame).c_str());
        myAppState->second = myAppState->second - 1000;
        myAppState->numFrame = 0;
    }

    // start render
    if(!renderer.startCommand()){
        SDL_Log("Error: renderer.startCommand");
        return SDL_APP_FAILURE;
    }
    if(!renderer.startRender()){
        SDL_Log("Error: renderer.startRender");
        return SDL_APP_FAILURE;
    }

    // render scene
    if(!sceneRenderer.ensureSceneExist()){
        SDL_Log("Error: sceneRenderer.ensureSceneExist");
        return SDL_APP_FAILURE;
    }
    if(!sceneRenderer.render()){
        SDL_Log("Error: sceneRenderer.render");
        return SDL_APP_FAILURE;
    }

    // end render
    if(!renderer.endRender()){
        SDL_Log("Error: renderer.endRender");
        return SDL_APP_FAILURE;
    }
    if(!renderer.endCommand()){
        SDL_Log("Error: renderer.endCommand");
        return SDL_APP_FAILURE;
    }

    // clean
    resourceManager.cleanResource();

    return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
    if(event->key.key == SDLK_SPACE && event->key.down){
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}
void SDL_AppQuit(void *appstate, SDL_AppResult result){
    if(result == SDL_AppResult::SDL_APP_SUCCESS){
        SDL_Log("App exit with SDL_APP_SUCCESS");
    }
    else if(result == SDL_AppResult::SDL_APP_FAILURE){
        SDL_Log("App exit with SDL_APP_FAILURE");
    }
    else{
        SDL_Log("App exit with Unkown");
    }

    core::SceneRenderer::GetInstance().release();
    core::ResourceManager::GetInstance().release();
    core::Renderer::GetInstance().release();
    MyAppState *myAppState = static_cast<MyAppState*>(appstate);
    delete myAppState;

    SDL_Log("App exit");
}
