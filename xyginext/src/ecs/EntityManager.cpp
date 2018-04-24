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

#include "xyginext/ecs/Entity.hpp"
#include "xyginext/core/Assert.hpp"
#include "xyginext/core/MessageBus.hpp"

using namespace xy;

namespace
{
    const std::size_t MinComponentMasks = 50;
}

EntityManager::EntityManager(MessageBus& mb)
    : m_messageBus  (mb),
    m_componentPools(Detail::MaxComponents)
{}

//public
Entity EntityManager::createEntity()
{
    Entity::ID idx;
    if (m_freeIDs.size() > Detail::MinFreeIDs)
    {
        idx = m_freeIDs.front();
        m_freeIDs.pop_front();
    }
    else
    {
        m_generations.push_back(0);
        idx = static_cast<Entity::ID>(m_generations.size() - 1);
        
        XY_ASSERT(idx < (1 << Detail::IndexBits), "Index out of range");
        if (idx >= m_componentMasks.size())
        {
            m_componentMasks.resize(m_componentMasks.size() + MinComponentMasks);
        }
    }

    XY_ASSERT(idx < m_generations.size(), "Index out of range");
    Entity e(idx, m_generations[idx]);
    e.m_entityManager = this;

    return e;
}

Entity EntityManager::createEntity(xy::Entity::ID id)
{
    Entity::ID idx(id);
    while(id >= m_generations.size())
    {
        m_generations.push_back(0);
    }
    idx = static_cast<Entity::ID>(m_generations.size() - 1);
        
    XY_ASSERT(idx < (1 << Detail::IndexBits), "Index out of range");
    if (idx >= m_componentMasks.size())
    {
        m_componentMasks.resize(m_componentMasks.size() + MinComponentMasks);
    }
    
    XY_ASSERT(idx < m_generations.size(), "Index out of range");
    Entity e(idx, m_generations[idx]);
    e.m_entityManager = this;
    
    return e;
}

void EntityManager::destroyEntity(Entity entity)
{
    const auto index = entity.getIndex();
    XY_ASSERT(index < m_generations.size(), "Index out of range");

    ++m_generations[index];
    m_freeIDs.push_back(index);
    m_componentMasks[index].reset();

    //let the world know the entity was destroyed
    auto msg = m_messageBus.post<Message::SceneEvent>(Message::SceneMessage);
    msg->entityID = index;
    msg->event = Message::SceneEvent::EntityDestroyed;
}

bool EntityManager::entityDestroyed(Entity entity) const
{
    const auto id = entity.getIndex();
    XY_ASSERT(id < m_generations.size(), "Generation index out of range");
    
    return (m_generations[id] != entity.getGeneration());
}

Entity EntityManager::getEntity(Entity::ID id) const
{
    XY_ASSERT(id < m_generations.size(), "Invalid Entity ID");
    Entity ent(id, m_generations[id]);
    ent.m_entityManager = const_cast<EntityManager*>(this);
    return ent;
}

const ComponentMask& EntityManager::getComponentMask(Entity entity) const
{
    const auto index = entity.getIndex();
    XY_ASSERT(index < m_componentMasks.size(), "Invalid mask index (out of range)");
    return m_componentMasks[index];
}

bool EntityManager::owns(Entity entity) const
{
    return (entity.m_entityManager == this);
}
