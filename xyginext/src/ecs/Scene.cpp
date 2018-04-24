/*********************************************************************
(c) Matt Marchant 2017 - 2018
http://trederia.blogspot.com

xygineXT - Zlib license.

This software is provided 'as-is', without any express or
implied warranty. In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.
*********************************************************************/

#include "xyginext/core/App.hpp"
#include "xyginext/core/Editor.hpp"
#include "xyginext/core/ConfigFile.hpp"
#include "xyginext/ecs/Scene.hpp"
#include "xyginext/ecs/components/Camera.hpp"
#include "xyginext/ecs/components/Transform.hpp"
#include "xyginext/ecs/components/AudioListener.hpp"
#include "xyginext/ecs/components/Sprite.hpp"

#include "xyginext/ecs/systems/RenderSystem.hpp"
#include "xyginext/ecs/systems/SpriteSystem.hpp"

#include <SFML/Window/Event.hpp>

#include "../imgui/imgui_dock.hpp"

#include "../cereal/archives/binary.hpp"

using namespace xy;

namespace
{
    sf::FloatRect getDefaultViewport()
    {
        if (App::getRenderTarget())
        {
            auto winSize = sf::Vector2f(App::getRenderTarget()->getSize());

            float windowRatio = winSize.x / winSize.y;
            float viewRatio = DefaultSceneSize.x / DefaultSceneSize.y;

            float sizeY = windowRatio / viewRatio;
            float top = (1.f - sizeY) / 2.f;

            return { { 0.f, top },{ 1.f, sizeY } };
        }
        return {};
    }
}

Scene::Scene(MessageBus& mb, const std::string& path)
    : m_messageBus      (mb),
    m_entityManager     (mb, m_componentManager),
    m_systemManager     (*this, m_componentManager)
{
    auto defaultCamera = createEntity();
    defaultCamera.addComponent<Transform>().setPosition(xy::DefaultSceneSize / 2.f);
    defaultCamera.addComponent<Camera>();
    defaultCamera.getComponent<Camera>().setViewport(getDefaultViewport());
    defaultCamera.addComponent<AudioListener>();

    m_defaultCamera = defaultCamera.getIndex();
    m_activeCamera = m_defaultCamera;
    m_activeListener = m_defaultCamera;

    currentRenderPath = [this](sf::RenderTarget& rt, sf::RenderStates states)
    {
        rt.setView(getEntity(m_activeCamera).getComponent<Camera>().m_view);
        for (auto r : m_drawables)
        {
            rt.draw(*r, states);
        }
    };
    
    // If a path has been passed, load it
    if (path.length())
    {
        loadFromFile(path);
    }
}

//public
void Scene::update(float dt)
{
    //update directors first as they'll be working on data from the last frame
    for (auto& d : m_directors)
    {
        d->process(dt);
    }

    for (const auto& entity : m_pendingEntities)
    {
        m_systemManager.addToSystems(entity);
    }
    m_pendingEntities.clear();

    for (const auto& entity : m_destroyedEntities)
    {
        m_systemManager.removeFromSystems(entity);
        m_entityManager.destroyEntity(entity);
    }
    m_destroyedEntities.clear();


    m_systemManager.process(dt);
    for (auto& p : m_postEffects) p->update(dt);
}

Entity Scene::createEntity()
{
    m_pendingEntities.push_back(m_entityManager.createEntity());
    return m_pendingEntities.back();
}

void Scene::destroyEntity(Entity entity)
{
    m_destroyedEntities.push_back(entity);
}

Entity Scene::getEntity(Entity::ID id) const
{
    return m_entityManager.getEntity(id);
}

void Scene::setPostEnabled(bool enabled)
{
    if (enabled && !m_postEffects.empty())
    {
        currentRenderPath = std::bind(&Scene::postRenderPath, this, std::placeholders::_1, std::placeholders::_2);
        
        XY_ASSERT(App::getRenderTarget(), "no valid window");
        auto size = App::getRenderTarget()->getSize();
        m_sceneBuffer.create(size.x, size.y, true);
        for (auto& p : m_postEffects) p->resizeBuffer(size.x, size.y);
    }
    else
    {       
        currentRenderPath = [this](sf::RenderTarget& rt, sf::RenderStates states)
        {
            rt.setView(getEntity(m_activeCamera).getComponent<Camera>().m_view);
            for (auto r : m_drawables)
            {
                rt.draw(*r, states);
            }
        };
    }
}

Entity Scene::getDefaultCamera() const
{
    return m_entityManager.getEntity(m_defaultCamera);
}

Entity Scene::setActiveCamera(Entity entity)
{
    XY_ASSERT(entity.hasComponent<Transform>() && entity.hasComponent<Camera>(), "Entity requires at least a transform and a camera component");
    XY_ASSERT(m_entityManager.owns(entity), "This entity must belong to this scene!");
    auto oldCam = m_entityManager.getEntity(m_activeCamera);
    m_activeCamera = entity.getIndex();

    return oldCam;
}

Entity Scene::setActiveListener(Entity entity)
{
    XY_ASSERT(entity.hasComponent<Transform>() && entity.hasComponent<AudioListener>(), "Entity requires at least a transform and a camera component");
    XY_ASSERT(m_entityManager.owns(entity), "This entity must belong to this scene!");
    auto oldListener = m_entityManager.getEntity(m_activeListener);
    m_activeListener = entity.getIndex();
    return oldListener;
}

Entity Scene::getActiveListener() const
{
    return m_entityManager.getEntity(m_activeListener);
}

Entity Scene::getActiveCamera() const
{
    return m_entityManager.getEntity(m_activeCamera);
}

void Scene::forwardEvent(const sf::Event& evt)
{
    for (auto& d : m_directors)
    {
        d->handleEvent(evt);
    }
}

void Scene::forwardMessage(const Message& msg)
{
    m_systemManager.forwardMessage(msg);
    for (auto& d : m_directors)
    {
        d->handleMessage(msg);
    }

    if (msg.id == Message::WindowMessage)
    {
        const auto& data = msg.getData<Message::WindowEvent>();
        if (data.type == Message::WindowEvent::Resized)
        {
            //update post effect buffers if they exist
            if (m_sceneBuffer.getTexture().getNativeHandle() > 0)
            {
                m_sceneBuffer.create(data.width, data.height);

                for (auto& b : m_postBuffers)
                {
                    if (b.getTexture().getNativeHandle() > 0)
                    {
                        b.create(data.width, data.height);
                    }
                }
            }
            //updates the view of the default camera
            getEntity(m_defaultCamera).getComponent<Camera>().setViewport(getDefaultViewport());
        }
    }
}

bool Scene::saveToFile(const std::string &path)
{
    std::ofstream os(path.c_str(), std::ios::binary | std::ios::trunc);
    cereal::BinaryOutputArchive archive(os);
    
    // Store systems first
    auto& systems = m_systemManager.getSystems();
    
    archive(systems.size());
    for (auto& s : systems)
    {
        archive(std::string(s->getType().name()));
    }
    
    // What to do when editor isn't active?
    // Add serialise functions to entity manager?
    auto entities = getSystem<EditorSystem>().getEntities();
    archive(entities.size()-1); // -1 for default entity
    
    for (auto& e : entities)
    {
        // First store component mask
        auto mask = e.getComponentMask();
        archive(mask.to_string());
        
        // Loop through components and store them
        // Need a smarter/generic way of doing this for all component types
        for (int i=0; i < mask.size(); i++)
        {
            if (mask.test(i))
            {
                if (i == Component::getID<Sprite>())
                {
                    archive(e.getComponent<Sprite>());
                }
            }
        }
    }
}

bool Scene::loadFromFile(const std::string &path)
{
    std::ifstream is(path.c_str());
    cereal::BinaryInputArchive archive(is);
    
    // Load systems first
    std::size_t systemCount;
    archive(systemCount);
    while(systemCount--)
    {
        // madness
        std::string name;
        archive(name);
        auto& mb = App::getActiveInstance()->getMessageBus();
        RenderSystem rs(mb);
        if (rs.getType().name() == name)
        {
            addSystem<RenderSystem>(mb);
        }
        SpriteSystem ss(mb);
        if (ss.getType().name() == name)
        {
            addSystem<SpriteSystem>(mb);
        }
        EditorSystem es(mb);
        if (es.getType().name() == name)
        {
            addSystem<EditorSystem>(mb);
        }
    }
    // Then entities
    std::size_t entityCount;
    archive(entityCount);
    while(entityCount--)
    {
        auto e = createEntity();
        
        std::string compMask;
        archive(compMask);
        ComponentMask mask(compMask);
        
        for (int i=0; i < mask.size(); i++)
        {
            if (mask.test(i))
            {
                if (i ==Component::getID<Sprite>())
                {
                    archive(e.addComponent<Sprite>());
                }
            }
        }
    }
}

//private
void Scene::postRenderPath(sf::RenderTarget& rt, sf::RenderStates states)
{
    m_sceneBuffer.setView(getEntity(m_activeCamera).getComponent<Camera>().m_view);

    m_sceneBuffer.clear();
    for (auto r : m_drawables)
    {
        m_sceneBuffer.draw(*r, states);
    }
    m_sceneBuffer.display();

    sf::RenderTexture* inTex = &m_sceneBuffer;
    sf::RenderTexture* outTex = nullptr;

    for (auto i = 0u; i < m_postEffects.size() - 1; ++i)
    {
        outTex = &m_postBuffers[i % 2];
        outTex->clear();
        m_postEffects[i]->apply(*inTex, *outTex);
        outTex->display();
        inTex = outTex;
    }

    rt.setView(inTex->getDefaultView());
    m_postEffects.back()->apply(*inTex, rt);
}

void Scene::draw(sf::RenderTarget& rt, sf::RenderStates states) const
{
    currentRenderPath(rt, states);
}
