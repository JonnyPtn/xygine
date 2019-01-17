/*********************************************************************
(c) Matt Marchant 2017 - 2019
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

#include "xyginext/ecs/components/Camera.hpp"
#include "xyginext/core/Assert.hpp"

#include <limits>

using namespace xy;

Camera::Camera()
    : m_lockAxis    (None),
    m_axisValue     (0.f),
    m_lockRotation  (false)
{
    m_view.setSize(DefaultSceneSize);
    m_view.setCenter(DefaultSceneSize / 2.f);

    auto maxFloat = std::numeric_limits<float>::max();
    m_bounds = { -maxFloat / 2.f, -maxFloat / 2.f, maxFloat, maxFloat };
}

//public
void Camera::setView(sf::Vector2f view)
{
    m_view.setSize(view);
}

void Camera::setViewport(sf::FloatRect viewport)
{
    m_view.setViewport(viewport);
}

void Camera::lockAxis(Axis axis, float position)
{
    m_lockAxis = axis;
    m_axisValue = position;
}

void Camera::lockRotation(bool lock)
{
    m_lockRotation = lock;
}

void Camera::setBounds(sf::FloatRect bounds)
{
    m_bounds = bounds;
}

void Camera::zoom(float zoom)
{
    XY_ASSERT(zoom > 0, "Value must be larger than 0");
    m_view.zoom(1.f / zoom);
}

sf::Vector2f Camera::getView() const
{
    return m_view.getSize();
}

sf::FloatRect Camera::getViewport() const
{
    return m_view.getViewport();
}

Camera::Axis Camera::getLockedAxis() const
{
    return m_lockAxis;
}

bool Camera::rotationLocked() const
{
    return m_lockRotation;
}
