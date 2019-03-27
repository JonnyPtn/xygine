/*********************************************************************
(c) Matt Marchant 2019

This file is part of the xygine tutorial found at
https://github.com/fallahn/xygine

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

#pragma once

#include <xyginext/ecs/Director.hpp>
#include <xyginext/ecs/components/ParticleEmitter.hpp>
#include <xyginext/ecs/Entity.hpp>

#include <vector>

namespace xy
{
    class ResourceHandler;
}

class FXDirector final : public xy::Director
{
public:
    explicit FXDirector(xy::ResourceHandler&);

    void handleEvent(const sf::Event&) override {}
    void handleMessage(const xy::Message&) override;
    void process(float) override;

private:
    xy::ResourceHandler& m_resources;
    xy::EmitterSettings m_particleSettings;

    std::vector<xy::Entity> m_entities;
    std::size_t m_nextFreeEntity;

    xy::Entity getNextFreeEntity();
    void resizeEntities(std::size_t);
    void doEffect(sf::Vector2f);
};