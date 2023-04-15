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

#include "Mighter2d/core/scene/SceneManager.h"
#include "Mighter2d/core/engine/Engine.h"
#include "Mighter2d/graphics/RenderTarget.h"
#include "Mighter2d/graphics/shapes/RectangleShape.h"
#include "Mighter2d/utility/Helpers.h"

namespace mighter2d::priv {
    namespace {
        void resetGui(ui::GuiContainer& gui) {
            // Reset focus state
            gui.unfocusAllWidgets();

            // Reset hover state
            SystemEvent event;
            event.type = SystemEvent::MouseMoved;
            event.mouseMove.x = -9999;
            gui.handleEvent(event);

            // Reset left mouse down state
            event.type = SystemEvent::MouseButtonReleased;
            event.mouseButton.button = input::Mouse::Button::Left;
            event.mouseButton.x = -9999;
            gui.handleEvent(event);
        }
    }

    SceneManager::SceneManager(Engine* engine) :
        engine_{engine},
        prevScene_{nullptr}
    {
        MIGHTER2D_ASSERT(engine, "Engine pointer cannot be a nullptr")

        engine->onFrameStart([this] {
            if (!scenes_.empty() && scenes_.top()->isEntered()) {
                Scene* activeScene = scenes_.top().get();
                Scene* bgScene = activeScene->getBackgroundScene();

                if (bgScene)
                    bgScene->onFrameBegin();

                activeScene->onFrameBegin();
            }
        });

        engine->onFrameEnd([this] {
            if (!scenes_.empty() && scenes_.top()->isEntered()) {
                Scene* activeScene = scenes_.top().get();
                Scene* bgScene = activeScene->getBackgroundScene();

                if (bgScene)
                    bgScene->onFrameEnd();

                activeScene->onFrameEnd();
            }
        });
    }

    void SceneManager::pushScene(Scene::Ptr scene, bool enterScene) {
        MIGHTER2D_ASSERT(scene, "Scene must not be a nullptr")
        if (!scenes_.empty()) {
            prevScene_ = scenes_.top().get();

            if (prevScene_->isEntered() && !prevScene_->isPaused()) {
                prevScene_->isPaused_ = true;
                prevScene_->isActive_ = false;
                resetGui(prevScene_->getGui());

                Scene* bgScene = prevScene_->getBackgroundScene();

                if (bgScene) {
                    resetGui(bgScene->getGui());
                    bgScene->isPaused_ = true;
                    prevScene_->isActive_ = false;
                    bgScene->onPause();
                }

                prevScene_->onPause();
            }
        }

        scenes_.push(std::move(scene));
        Scene* activeScene = scenes_.top().get();

        if (activeScene->isEntered()) {
            if (activeScene->isCached()) {
                activeScene->isPaused_ = false;
                activeScene->isActive_ = true;
                Scene* bgScene = activeScene->getBackgroundScene();

                if (bgScene) {
                    bgScene->isPaused_ = false;
                    bgScene->isActive_ = true;
                    bgScene->onResumeFromCache();
                }

                activeScene->onResumeFromCache();
            }
        } else if (enterScene) {
            activeScene->init(*engine_);
            activeScene->isEntered_ = true;
            activeScene->isActive_ = true;
            activeScene->onEnter();
        }
    }

    Scene::Ptr SceneManager::popCached(const std::string &name) {
        auto found = cachedScenes_.find(name);

        if (found != cachedScenes_.end()) {
            Scene::Ptr scene = std::move(found->second);
            cachedScenes_.erase(found);
            return scene;
        }

        return nullptr;
    }

    Scene *SceneManager::getCached(const std::string &name) {
        return const_cast<Scene*>(std::as_const(*this).getCached(name));
    }

    const Scene *SceneManager::getCached(const std::string &name) const {
        if (isCached(name))
            return cachedScenes_.at(name).get();
        else
            return nullptr;
    }

    void SceneManager::cache(const std::string &name, Scene::Ptr scene) {
        MIGHTER2D_ASSERT(scene, "Cached scene must not be a nullptr")
        auto [iter, inserted] = cachedScenes_.insert(std::pair{name, std::move(scene)});

        if (inserted) {
            iter->second->setCached(true, name);

            Scene* bgScene = iter->second->getBackgroundScene();

            if (bgScene) {
                bgScene->setCached(true, name + "BackgroundScene");
                bgScene->onCache();
            }

            iter->second->onCache();
        }
    }

    bool SceneManager::isCached(const std::string &name) const {
        return cachedScenes_.find(name) != cachedScenes_.end();
    }

    bool SceneManager::removeCached(const std::string &name) {
        std::size_t sizeBefore = cachedScenes_.size();
        cachedScenes_.erase(name);
        return cachedScenes_.size() < sizeBefore;
    }

    void SceneManager::popScene(bool resumePrev) {
        if (scenes_.empty())
            return;

        prevScene_ = nullptr;

        // Call onExit() after removing state from container because onExit may
        // push a scene and the new scene will be removed instead of this one
        Scene::Ptr poppedScene = std::move(scenes_.top());
        scenes_.pop();

        // Notify popped scene that it's removed from the engine
        if (poppedScene->isEntered()) {
            Scene* bgScene = poppedScene->getBackgroundScene();

            if (bgScene)
                bgScene->onExit();

            poppedScene->onExit();
        }

        // Attempt to cache the removed scene
        if (const auto& [isCached, cacheAlias] = poppedScene->cacheState_; isCached) {
            resetGui(poppedScene->getGui());
            poppedScene->isActive_ = false;

            if (Scene* bgScene = poppedScene->getBackgroundScene(); bgScene) {
                resetGui(bgScene->getGui());
                bgScene->isActive_ = false;
            }

            cache(cacheAlias, std::move(poppedScene));
        }

        // Activate a new scene
        if (!scenes_.empty()) {
            if (scenes_.size() >= 2) {
                Scene::Ptr currentScene = std::move(scenes_.top());
                scenes_.pop();
                prevScene_ = scenes_.top().get();
                scenes_.push(std::move(currentScene));
            }

            Scene* activeScene = scenes_.top().get();

            if (activeScene->isEntered() && resumePrev) {
                activeScene->isPaused_ = false;
                activeScene->isActive_ = true;

                Scene* bgScene = activeScene->getBackgroundScene();

                if (bgScene) {
                    bgScene->isPaused_ = false;
                    bgScene->isActive_ = true;
                    bgScene->onResume();
                }

                activeScene->onResume();
            } else if (resumePrev) {
                activeScene->init(*engine_);
                activeScene->isEntered_ = true;
                activeScene->isActive_ = true;
                activeScene->onEnter();
            }
        }
    }

    Scene* SceneManager::getActiveScene() {
        return const_cast<Scene*>(std::as_const(*this).getActiveScene());
    }

    const Scene *SceneManager::getActiveScene() const {
        if (scenes_.empty() || !scenes_.top()->isEntered())
            return nullptr;
        else
            return scenes_.top().get();
    }

    Scene *SceneManager::getPreviousScene() {
        return const_cast<Scene*>(std::as_const(*this).getPreviousScene());
    }

    const Scene *SceneManager::getPreviousScene() const {
        if (prevScene_ && prevScene_->isEntered())
            return prevScene_;

        return nullptr;
    }

    Scene *SceneManager::getBackgroundScene() {
        return const_cast<Scene*>(std::as_const(*this).getBackgroundScene());
    }

    const Scene *SceneManager::getBackgroundScene() const {
        const Scene* activeScene = getActiveScene();

        if (activeScene)
            return activeScene->getBackgroundScene();
        else
            return nullptr;
    }

    std::size_t SceneManager::getSceneCount() const {
        return scenes_.size();
    }

    void SceneManager::clear() {
        prevScene_ = nullptr;
        while (!scenes_.empty())
            scenes_.pop();
    }

    void SceneManager::clearCachedScenes() {
        cachedScenes_.clear();
    }

    void SceneManager::clearAllExceptActive() {
        if (!scenes_.empty()) {
            if (scenes_.top()->isEntered()) {
                Scene::Ptr activeScene = std::move(scenes_.top());
                clear();
                scenes_.push(std::move(activeScene));
            } else
                clear();
        }
    }

    void SceneManager::enterTopScene() const {
        if (scenes_.empty())
            return;

        if (!scenes_.top()->isEntered()) {
            scenes_.top()->init(*engine_);
            scenes_.top()->isEntered_ = true;
            scenes_.top()->onEnter();
        }
    }

    bool SceneManager::isEmpty() const {
        return scenes_.empty();
    }

    void SceneManager::render(priv::RenderTarget &window) {
        auto static renderScene = [](Scene* scene, Camera* camera, priv::RenderTarget& renderWindow) {
            scene->onPreRender();

            if (!camera->isDrawable())
                return;

            // Reset view so that the scene can be rendered on the current camera
            const sf::View& view = std::any_cast<std::reference_wrapper<const sf::View>>(camera->getInternalView()).get();
            renderWindow.getThirdPartyWindow().setView(view);

            scene->renderLayers_.render(renderWindow);

            scene->onPostRender();
        };

        // Render the scene on each camera to update its view
        auto static renderEachCam = [](Scene* scene, priv::RenderTarget& renderTarget) {
            // Render main/default camera
            renderScene(scene, &scene->getCamera(), renderTarget);
        };

        if (!scenes_.empty() && scenes_.top()->isEntered()) {
            // Render previous scene
            if (prevScene_ && prevScene_->isEntered() && prevScene_->isVisibleOnPause()) {
                Scene* bgScene = prevScene_->getBackgroundScene();

                if (bgScene && prevScene_->isBackgroundSceneDrawable())
                    renderEachCam(bgScene, window);

                renderEachCam(prevScene_, window);
            }

            // Render the active scenes background scene
            Scene* activeScene = scenes_.top().get();
            Scene* bgScene = activeScene->getBackgroundScene();

            if(bgScene && activeScene->isBackgroundSceneDrawable())
                renderEachCam(bgScene, window);

            // Render the active scene
            renderEachCam(activeScene, window);
        }
    }

    void SceneManager::update(Time deltaTime) {
        update(deltaTime, false);
    }

    void SceneManager::handleEvent(SystemEvent event) {
        if (scenes_.empty())
            return;

        if (!scenes_.top()->isEntered())
            return;

        // Handle a camera's response to a window resize event
        static auto updateCameraScale = [](Camera* camera, unsigned int windowWidth, unsigned int windowHeight) {
            Camera::OnWinResize response = camera->getWindowResizeResponse();

            if (response == Camera::OnWinResize::Letterbox) {
                const sf::View& view = std::any_cast<std::reference_wrapper<const sf::View>>(camera->getInternalView()).get();
                camera->setInternalView(std::any{utility::letterbox(view, windowWidth, windowHeight)});
            } else if (response == Camera::OnWinResize::MaintainSize)
                camera->setInternalView(std::any{sf::View(sf::FloatRect(0, 0, static_cast<float>(windowWidth), static_cast<float>(windowHeight)))});
        };

        // Update all system components of a scene
        static auto updateSystem = [](Scene* scene, SystemEvent e) {
            if (e.type == SystemEvent::Resized) {
                updateCameraScale(&scene->getCamera(), e.size.width, e.size.height);
            }

            // Absorb key event if Keyboard is disabled
            if (!scene->inputManager_.isInputEnabled(input::InputType::Keyboard) &&
                (e.type == SystemEvent::KeyPressed || e.type == SystemEvent::KeyReleased))
            {
                return;
            }

            // Absorb mouse event if the Mouse is disabled
            if (!scene->inputManager_.isInputEnabled(input::InputType::Mouse) &&
                (e.type == SystemEvent::MouseButtonPressed || e.type == SystemEvent::MouseButtonReleased
                 || e.type == SystemEvent::MouseMoved || e.type == SystemEvent::MouseWheelScrolled))
            {
                return;
            }

            // Absorb joystick event if Joystick is disabled
            if (!scene->inputManager_.isInputEnabled(input::InputType::Joystick) &&
                (e.type == SystemEvent::JoystickButtonPressed || e.type == SystemEvent::JoystickButtonReleased ||
                 e.type == SystemEvent::JoystickMoved))
            {
                return;
            }

            for (auto& sysEventHandler : scene->systemEventHandlerList_) {
                sysEventHandler->handleEvent(e);
            }

            scene->onHandleEvent(e);
        };

        Scene* activeScene = scenes_.top().get();
        Scene* bgScene = activeScene->getBackgroundScene();

        if (bgScene && activeScene->isBackgroundSceneEventsEnabled())
            updateSystem(bgScene, event);

        updateSystem(activeScene, event);
    }

    void SceneManager::fixedUpdate(Time deltaTime) {
        update(deltaTime, true);
    }

    void SceneManager::forEachScene(const Callback<const Scene::Ptr&>& callback) {
        std::stack<Scene::Ptr> temp;
        while (!scenes_.empty()) {
            callback(scenes_.top());
            temp.push(std::move(scenes_.top()));
            scenes_.pop();
        }

        while (!temp.empty()) {
            scenes_.push(std::move(temp.top()));
            temp.pop();
        }
    }

    void SceneManager::preUpdate(Time deltaTime) {
        if (scenes_.empty())
            return;

        if (!scenes_.top()->isEntered())
            return;

        //@todo Remove - Classes must be able to react to a frame end event directly
        static auto update = [](Scene* scene, Time dt) {
            scene->timerManager_.preUpdate();
            scene->audioManager_.removePlayedAudio();
        };

        Scene* activeScene = scenes_.top().get();

        // Update the active scenes background scene
        Scene* bgScene = activeScene->getBackgroundScene();

        if (bgScene && activeScene->isBackgroundSceneUpdateEnabled()) {
            update(bgScene, deltaTime);
            bgScene->onPreUpdate(deltaTime * bgScene->getTimescale());
        }

        // Update active scene
        update(activeScene, deltaTime);
        activeScene->onPreUpdate(deltaTime * activeScene->getTimescale());
    }

    void SceneManager::update(const Time &deltaTime, bool fixedUpdate) {
        if (!scenes_.empty() && scenes_.top()->isEntered()) {
            Scene* activeScene = scenes_.top().get();
            Scene* bgScene = activeScene->getBackgroundScene();

            if (bgScene && activeScene->isBackgroundSceneUpdateEnabled())
                updateScene(deltaTime, bgScene, fixedUpdate);

            updateScene(deltaTime, activeScene, fixedUpdate);
        }
    }

    void SceneManager::updateScene(const Time& deltaTime, Scene* scene, bool fixedUpdate) {
        if (fixedUpdate) {
            for (auto& updatable : scene->updateList_) {
                updatable->fixedUpdate(deltaTime * scene->getTimescale());
            }

            scene->onFixedUpdate(deltaTime * scene->getTimescale());
        }
        else {
            if (scene->grid2D_) {
                scene->grid2D_->update(deltaTime * scene->getTimescale());
            }

            // Normal update
            for (auto& updatable : scene->updateList_) {
                updatable->update(deltaTime * scene->getTimescale());
            }

            // Update user scene after all internal updates
            scene->onUpdate(deltaTime * scene->getTimescale());

            // Normal update is always called after fixed update: fixedUpdate -> update -> postUpdate
            scene->onPostUpdate(deltaTime * scene->getTimescale());
        }
    }

    SceneManager::~SceneManager() {
        prevScene_ = nullptr;
    }
}
