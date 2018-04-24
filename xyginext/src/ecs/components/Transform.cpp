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

#include "xyginext/ecs/components/Transform.hpp"
#include "xyginext/core/Assert.hpp"

using namespace xy;

Transform::Transform()
    : m_parent  (nullptr),
    m_children(),
    m_depth     (0)
{

}

Transform::~Transform()
{
    //remove this transform from its parent
    if (m_parent)
    {
        auto& siblings = m_parent->m_children;
        siblings.erase(
            std::remove_if(siblings.begin(), siblings.end(),
                           [this](const std::shared_ptr<Transform> ptr)
        {return ptr.get() == this; }),
            siblings.end());
    }

    //orphan any children
    for(auto c : m_children)
    {
        c->m_parent = nullptr;
        c->setDepth(0);
    }
}

Transform::Transform(Transform&& other)
{
    if (other.m_parent.get() != this)
    {
        m_parent = other.m_parent;
        m_depth = other.m_depth;

        other.m_parent = nullptr;
        other.setDepth(0);

        //swap ourself into siblings list
        if (m_parent)
        {
            auto& siblings = m_parent->m_children;
            for (auto& s : siblings)
            {
                if (s.get() == &other)
                {
                    s.reset(this);
                    break;
                }
            }
        }
        m_children = std::move(other.m_children);

        //update the children's new parent
        for (auto c : m_children)
        {
            if (c.get() == this)
            {
                throw std::runtime_error("Transform already exists in child list");
            }

            c->m_parent.reset(this);
            c->setDepth(m_depth + 1);
        }

        //actually take on the other transform
        setPosition(other.getPosition());
        setRotation(other.getRotation());
        setScale(other.getScale());
        setOrigin(other.getOrigin());

        other.setPosition({});
        other.setRotation(0.f);
        other.setScale({ 1.f, 1.f });
        other.setOrigin({});
    }
    else
    {
        xy::Logger::log("Move construction tried to parent transform to itself");
    }
}

Transform& Transform::operator=(Transform&& other)
{
    if (&other != this && other.m_parent.get() != this)
    {
        m_parent = other.m_parent;
        m_depth = other.m_depth;

        other.m_parent = nullptr;
        other.setDepth(0);

        //swap ourself into siblings list
        if (m_parent)
        {
            auto& siblings = m_parent->m_children;
            for (auto& s : siblings)
            {
                if (s.get() == &other)
                {
                    s.reset(this);
                    break;
                }
            }
        }

        m_children = std::move(other.m_children);

        //update the children's new parent
        for (auto c : m_children)
        {
            if (c.get() == this)
            {
                throw std::runtime_error("Transform exists in list of children");
            }
            c->m_parent.reset(this);
            c->setDepth(m_depth + 1);
        }

        //actually take on the other transform
        setPosition(other.getPosition());
        setRotation(other.getRotation());
        setScale(other.getScale());
        setOrigin(other.getOrigin());

        other.setPosition({});
        other.setRotation(0.f);
        other.setScale({ 1.f, 1.f });
        other.setOrigin({});
    }
    return *this;
}

//public
void Transform::addChild(Transform& child)
{
    XY_ASSERT(this != &child, "Can't parent to ourself!");

    if (&child == this)
    {
        xy::Logger::log("Cannot parent transform to itself");
        return;
    }

    //remove old parent first
    if (child.m_parent)
    {
        if (child.m_parent.get() == this)
        {
            return; //already added!
        }

        auto& otherSiblings = child.m_parent->m_children;
        otherSiblings.erase(std::remove_if(otherSiblings.begin(), otherSiblings.end(),
                                           [&child](const std::shared_ptr<Transform> ptr)
        {
            return ptr.get() == &child;
        }), otherSiblings.end());
    }
    child.m_parent.reset(this);
    child.setDepth(m_depth + 1);
    m_children.push_back(std::make_shared<Transform>(std::move(child)));
}

void Transform::removeChild(Transform& tx)
{
    if (tx.m_parent.get() != this) return;

    tx.m_parent = nullptr;
    tx.setDepth(0);

    m_children.erase(std::remove_if(m_children.begin(), m_children.end(), 
                                    [&tx](const std::shared_ptr<Transform> ptr)
    {
        return ptr.get() == &tx;
    }), m_children.end());
}

sf::Transform Transform::getWorldTransform() const
{
    if (m_parent)
    {
        return m_parent->getWorldTransform() * getTransform();
    }
    return getTransform();
}

sf::Vector2f Transform::getWorldPosition() const
{
    return getWorldTransform().transformPoint({});
}

//private
void Transform::setDepth(std::size_t depth)
{
    m_depth = depth;
    for (auto& c : m_children)
    {
        c->setDepth(depth + 1);
    }

    XY_WARNING(m_depth > 10, "Transform added with depth " + std::to_string(m_depth));
}
