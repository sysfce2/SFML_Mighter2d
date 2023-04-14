////////////////////////////////////////////////////////////////////////////////
// Mighter2d
//
// Copyright (c) 2023 Kwena Mashamaite
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include "Mighter2d/core/scene/Scene.h"
#include "Mighter2d/core/engine/Engine.h"
#include "Mighter2d/core/exceptions/Exceptions.h"
#include "Mighter2d/utility/Helpers.h"

namespace mighter2d {
    Scene::Scene() :
        timerManager_(*this),
        timescale_{1.0f},
        isEntered_{false},
        isInitialized_{false},
        isPaused_{false},
        isVisibleWhenPaused_{false},
        isBackgroundSceneDrawable_{true},
        isBackgroundSceneUpdated_{true},
        isBackgroundSceneEventsEnabled_{false},
        hasGrid2D_{false},
        cacheState_{false, ""},
        parentScene_{nullptr},
        spriteContainer_{std::make_unique<SpriteContainer>(renderLayers_)},
        entityContainer_{std::make_unique<GameObjectContainer>(renderLayers_)},
        shapeContainer_{std::make_unique<ShapeContainer>(renderLayers_)}
    {
        renderLayers_.create("default");
    }

    Scene::Scene(Scene&& other) noexcept :
        timerManager_(std::move(other.timerManager_))
    {
        *this = std::move(other);
    }

    Scene &Scene::operator=(Scene&& other) noexcept {
        // We can't use a default move assignment operator because of reference members
        if (this != &other) {
            Object::operator=(std::move(other));
            engine_ = std::move(other.engine_);
            window_ = std::move(other.window_);
            camera_ = std::move(other.camera_);
            cache_ = std::move(other.cache_);
            sCache_ = std::move(other.sCache_);
            inputManager_ = std::move(other.inputManager_);
            audioManager_ = std::move(other.audioManager_);
            timerManager_ = std::move(other.timerManager_);
            guiContainer_ = std::move(other.guiContainer_);
            renderLayers_ = std::move(other.renderLayers_);
            entityContainer_ = std::move(other.entityContainer_);
            gridMovers_ = std::move(other.gridMovers_);
            shapeContainer_ = std::move(other.shapeContainer_);
            grid2D_ = std::move(other.grid2D_);
            timescale_ = other.timescale_;
            isVisibleWhenPaused_ = other.isVisibleWhenPaused_;
            isBackgroundSceneDrawable_ = other.isBackgroundSceneDrawable_;
            isBackgroundSceneUpdated_ = other.isBackgroundSceneUpdated_;
            isBackgroundSceneEventsEnabled_ = other.isBackgroundSceneEventsEnabled_;
            hasGrid2D_ = other.hasGrid2D_;
            cacheState_ = other.cacheState_;
            isEntered_ = other.isEntered_;
            isInitialized_ = other.isInitialized_;
            isPaused_ = other.isPaused_;
            parentScene_ = other.parentScene_;
            backgroundScene_ = std::move(other.backgroundScene_);
        }

        return *this;
    }

    Scene::Ptr Scene::create() {
        return std::make_unique<Scene>();
    }

    void Scene::init(Engine &engine) {
        if (!isInitialized_) {
            isInitialized_ = true;
            engine_ = std::make_unique<std::reference_wrapper<Engine>>(engine);
            window_ = std::make_unique<std::reference_wrapper<Window>>(engine.getWindow());
            cameraContainer_ = std::make_unique<CameraContainer>(engine.getRenderTarget());
            camera_ = std::make_unique<Camera>(engine.getRenderTarget());
            cache_ = std::make_unique<std::reference_wrapper<PropertyContainer>>(engine.getCache());
            sCache_ = std::make_unique<std::reference_wrapper<PrefContainer>>(engine.getSavableCache());
            guiContainer_ = std::make_unique<ui::GuiContainer>(*this);
            guiContainer_->setTarget(engine.getRenderTarget());
            onInit();
        }
    }

    void Scene::addUpdatable(IUpdatable *updatable) {
        if (!utility::findIn(updateList_, updatable).first) {
            updateList_.push_back(updatable);
        }
    }

    bool Scene::removeUpdatable(IUpdatable *updatable) {
        return utility::eraseIn(updateList_, updatable);
    }

    std::string Scene::getClassName() const {
        return "Scene";
    }

    void Scene::setCached(bool cache, const std::string& alias) {
        cacheState_.first = cache;
        cacheState_.second = alias;
    }

    bool Scene::isCached() const {
        return cacheState_.first;
    }

    void Scene::setVisibleOnPause(bool visible) {
        if (isVisibleWhenPaused_ != visible) {
            isVisibleWhenPaused_ = visible;

            emitChange(Property("visibleOnPause", isVisibleWhenPaused_));
        }
    }

    bool Scene::isVisibleOnPause() const {
        return isVisibleWhenPaused_;
    }

    void Scene::setBackgroundScene(Scene::Ptr scene) {
        if (!isInitialized_)
            throw AccessViolationException("mighter2d::Scene::setBackgroundScene() must not be called before the parent scene is initialized");

        if (!isEntered_)
            throw AccessViolationException("mighter2d::Scene::setBackgroundScene() must not be called before the parent scene is entered");

        if (isBackgroundScene())
            throw AccessViolationException("mighter2d::Scene::setBackgroundScene() must not be called on a background scene, nested background scenes are not supported");

        if (scene) {
            if (scene->isBackgroundScene())
                throw AccessViolationException("mighter2d::Scene::setBackgroundScene() must not be called with a scene that is already a background of another scene");

            if (scene->hasBackgroundScene())
                throw AccessViolationException("mighter2d::Scene::setBackgroundScene() must not be called with a scene that has a background scene, nested background scenes are not supported");
        }

        if (backgroundScene_ != scene) {
            if (backgroundScene_)
                backgroundScene_->onExit();

            backgroundScene_ = std::move(scene);

            if (backgroundScene_) {
                backgroundScene_->parentScene_ = this;
                backgroundScene_->init(*engine_);

                backgroundScene_->isEntered_ = true;
                backgroundScene_->onEnter();
            }
        }
    }

    Scene *Scene::getBackgroundScene() {
        return backgroundScene_.get();
    }

    const Scene *Scene::getBackgroundScene() const {
        return backgroundScene_.get();
    }

    Scene *Scene::getParentScene() {
        return parentScene_;
    }

    const Scene *Scene::getParentScene() const {
        return parentScene_;
    }

    bool Scene::isBackgroundScene() const {
        return parentScene_ != nullptr;
    }

    bool Scene::hasBackgroundScene() const {
        return backgroundScene_ != nullptr;
    }

    void Scene::setBackgroundSceneDrawable(bool drawable) {
        if (isBackgroundSceneDrawable_ != drawable) {
            isBackgroundSceneDrawable_ = drawable;

            emitChange(Property("backgroundSceneDrawable", drawable));
        }
    }

    bool Scene::isBackgroundSceneDrawable() const {
        return isBackgroundSceneDrawable_;
    }

    void Scene::setBackgroundSceneUpdateEnable(bool enable) {
        if (isBackgroundSceneUpdated_ != enable) {
            isBackgroundSceneUpdated_ = enable;

            emitChange(Property("backgroundSceneUpdateEnable", isBackgroundSceneUpdated_));
        }
    }

    bool Scene::isBackgroundSceneUpdateEnabled() const {
        return isBackgroundSceneUpdated_;
    }

    void Scene::setBackgroundSceneEventsEnable(bool enable) {
        if (isBackgroundSceneEventsEnabled_ != enable) {
            isBackgroundSceneEventsEnabled_ = enable;

            emitChange(Property("backgroundSceneEventsEnable", enable));
        }
    }

    bool Scene::isBackgroundSceneEventsEnabled() const {
        return isBackgroundSceneEventsEnabled_;
    }

    bool Scene::isEntered() const {
        return isEntered_;
    }

    bool Scene::isPaused() const {
        return isPaused_;
    }

    void Scene::setTimescale(float timescale) {
        if (timescale_ == timescale)
            return;

        if (timescale < 0)
            timescale_ = 0.0f;
        else
            timescale_ = timescale;

        emitChange(Property{"timescale", timescale});
    }

    float Scene::getTimescale() const {
        return timescale_;
    }

    Engine &Scene::getEngine() {
        return const_cast<Engine&>(std::as_const(*this).getEngine());
    }

    const Engine &Scene::getEngine() const {
        if (!engine_)
            throw AccessViolationException("mighter2d::Scene::getEngine() must not be called before the scene is initialized");
        else
            return *engine_;
    }

    Window &Scene::getWindow() {
        return const_cast<Window&>(std::as_const(*this).getWindow());
    }

    const Window &Scene::getWindow() const {
        if (!window_)
            throw AccessViolationException("mighter2d::Scene::getWindow() must not be called before the scene is initialized");
        else
            return *window_;
    }

    Camera &Scene::getCamera() {
        return const_cast<Camera&>(std::as_const(*this).getCamera());
    }

    const Camera &Scene::getCamera() const {
        if (!camera_)
            throw AccessViolationException("mighter2d::Scene::getCamera() must not be called before the scene is initialized");
        else
            return *camera_;
    }

    CameraContainer &Scene::getCameras() {
        return const_cast<CameraContainer&>(std::as_const(*this).getCameras());
    }

    const CameraContainer &Scene::getCameras() const {
        if (!cameraContainer_)
            throw AccessViolationException("mighter2d::Scene::getCameras() must not be called before the scene is initialized");
        else
            return *cameraContainer_;
    }

    GridMoverContainer &Scene::getGridMovers() {
        return gridMovers_;
    }

    const GridMoverContainer &Scene::getGridMovers() const {
        return gridMovers_;
    }

    input::InputManager &Scene::getInput() {
        return inputManager_;
    }

    const input::InputManager &Scene::getInput() const {
        return inputManager_;
    }

    audio::AudioManager &Scene::getAudio() {
        return audioManager_;
    }

    const audio::AudioManager &Scene::getAudio() const {
        return audioManager_;
    }

    TimerManager &Scene::getTimer() {
        return timerManager_;
    }

    const TimerManager &Scene::getTimer() const {
        return timerManager_;
    }

    PropertyContainer &Scene::getCache() {
        return const_cast<PropertyContainer&>(std::as_const(*this).getCache());
    }

    const PropertyContainer &Scene::getCache() const {
        if (!cache_)
            throw AccessViolationException("mighter2d::Scene::getCache() must not be called before the scene is initialized");
        else
            return *cache_;
    }

    PrefContainer &Scene::getSCache() {
        return const_cast<PrefContainer&>(std::as_const(*this).getSCache());
    }

    const PrefContainer &Scene::getSCache() const {
        if (!sCache_)
            throw AccessViolationException("mighter2d::Scene::getSCache() must not be called before the scene is initialized");
        else
            return *sCache_;
    }

    RenderLayerContainer &Scene::getRenderLayers() {
        return renderLayers_;
    }

    const RenderLayerContainer &Scene::getRenderLayers() const {
        return renderLayers_;
    }

    Grid &Scene::getGrid() {
        return const_cast<Grid&>(std::as_const(*this).getGrid());
    }

    const Grid &Scene::getGrid() const {
        if (!grid2D_)
            throw AccessViolationException("mighter2d::Scene::createGrid2D() must be called first before calling mighter2d::Scene::getGrid()");
        else
            return *grid2D_;
    }

    ui::GuiContainer &Scene::getGui() {
        return const_cast<ui::GuiContainer&>(std::as_const(*this).getGui());
    }

    const ui::GuiContainer &Scene::getGui() const {
        if (!guiContainer_->isTargetSet())
            throw AccessViolationException("mighter2d::Scene::getGui() must not be called before the scene is initialized");
        else
            return *guiContainer_;
    }

    ShapeContainer &Scene::getShapes() {
        return *shapeContainer_;
    }

    const ShapeContainer &Scene::getShapes() const {
        return *shapeContainer_;
    }

    GameObjectContainer &Scene::getGameObjects() {
        return *entityContainer_;
    }

    const GameObjectContainer &Scene::getGameObjects() const {
        return *entityContainer_;
    }

    SpriteContainer &Scene::getSprites() {
        return *spriteContainer_;
    }

    const SpriteContainer &Scene::getSprites() const {
        return *spriteContainer_;
    }

    void Scene::createGrid2D(unsigned int tileWidth, unsigned int tileHeight) {
        grid2D_ = std::make_unique<Grid>(tileWidth, tileHeight, *this);
        hasGrid2D_ = true;
    }

    Scene::~Scene() {
        emitDestruction();
    }
}