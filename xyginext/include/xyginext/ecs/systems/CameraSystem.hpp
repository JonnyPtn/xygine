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

#ifndef XY_CAMERA_SYSTEM_HPP_
#define XY_CAMERA_SYSTEM_HPP_

#include "xyginext/ecs/System.hpp"

namespace xy
{
    /*!
    \brief The camera system updates any camera components
    which are attached to entities that also have a transform.
    Properties such as locked rotation or locked axis are
    applied by this system, so it needs to exist in a scene
    which uses any camera other than the default one.
    \see Camera
    */
    class XY_EXPORT_API CameraSystem : public System
    {
    public:
        explicit CameraSystem(MessageBus&);

        void process(float) override;
    };
}

#endif //XY_CAMERA_SYSTEM_HPP_
