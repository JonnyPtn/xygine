/*********************************************************************
Matt Marchant 2014 - 2016
http://trederia.blogspot.com

xygine - Zlib license.

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

#ifndef XY_MESH_WATER_HPP_
#define XY_MESH_WATER_HPP_

#include <string>

namespace xy
{
    namespace Shader
    {
        namespace Mesh
        {
            static const std::string WaterFragment =
                "#version 120\n"

                "uniform sampler2D u_diffuseMap;\n"
                "uniform sampler2D u_positionMap;\n"

                "uniform sampler2D u_surfaceMap;\n"

                "uniform float u_time;\n"

                "uniform float u_waterLevel = 250.0;\n"
                "//uniform float u_nearPlane = 50.0;\n"

                "uniform vec3 u_cameraWorldPosition;\n"
                "//uniform mat4 u_invViewMatrix;\n"

                /*this is a kludge because of the inverted Y coord*/
                "const float sceneHeight = 1080.0;\n"
                "const float tau = 6.2831;\n"
                "void main()\n"
                "{\n"
                "    float waterLevel = sceneHeight - u_waterLevel;\n"

                "    vec4 position = texture2D(u_positionMap, vec2(gl_TexCoord[0].x, 1.0 - gl_TexCoord[0].y));\n"

                "    if(u_cameraWorldPosition.y < u_waterLevel)\n"
                "    {\n"
                "        //TODO distance based fogging, and rendering surface as seen from below\n"
                "        gl_FragColor = texture2D(u_diffuseMap, gl_TexCoord[0].xy);\n"
                "        return;\n"
                "    }\n"

                /*remember SFML Y coord is inverted!*/
                "    if(position.y > waterLevel)\n"
                "    {\n"
                "        vec3 eyeVec = normalize(position.xyz - u_cameraWorldPosition);\n"
                "        float t = (u_waterLevel - position.y) / eyeVec.y;\n"
                "        vec3 surfacePoint = u_cameraWorldPosition - eyeVec * t;\n"
                "        vec2 texCoord = (surfacePoint.xz + eyeVec.xz * 0.1) * 0.001;\n"

                "        float cycle = texture2D(u_surfaceMap, texCoord).r * tau;\n"
                "        cycle = sin((0.75 * u_time) + cycle) * 0.5 + 0.5;\n"
                "        vec3 surface = vec3(cycle);\n"

                "        float depth = clamp(surfacePoint.y - position.y, 0.0, 1.0);\n"
                "        float refractOffset = sin((gl_TexCoord[0].y * 50.0 + u_time)) * (0.001 * depth);\n"
                "        texCoord = gl_TexCoord[0].xy + refractOffset;\n"

                "        vec4 refractPosition = texture2D(u_positionMap, vec2(texCoord.x, 1.0 - texCoord.y));\n"
                "        vec4 refraction = texture2D(u_diffuseMap, texCoord);\n"
                "        refraction.rgb *= vec3(0.6, 0.7, 0.9) * (1.0 - refractPosition.a) * depth;\n"

                /*the camera is fixed along the z-axis so we can save time by flipping the buffer vertically*/
                "        float reflectionOffset = ((waterLevel - u_waterLevel) / sceneHeight);\n"
                "        vec3 reflectPosition = texture2D(u_positionMap, vec2(gl_TexCoord[0].x, gl_TexCoord[0].y + reflectionOffset)).rgb;\n"
                "        vec4 reflection = vec4(0.0);\n"
                "        if(reflectPosition.y <= waterLevel - 4.0)\n"
                "        {\n"
                "            reflection = texture2D(u_diffuseMap, vec2(gl_TexCoord[0].x, 1.0 - (gl_TexCoord[0].y + reflectionOffset)));\n"
                "        }\n"

                "        vec4 colour = refraction;\n"
                "        if(gl_TexCoord[0].y > 0.2)\n"
                "        {\n"
                "            colour.rgb = mix(refraction.rgb, reflection.rgb, (1.0 - refractPosition.a));\n"
                "        }\n"

                "        gl_FragColor = colour;\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        gl_FragColor = texture2D(u_diffuseMap, gl_TexCoord[0].xy);\n"
                "    }\n"
                "}";
        }
    }
}

#endif //XY_MESH_WATER_HPP_