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

#ifndef XY_SHADER_NORMALMAPPED_HPP_
#define XY_SHADER_NORMALMAPPED_HPP_

#include <string>

namespace xy
{
    namespace Shader
    {
        namespace NormalMapped
        {
            static const size_t MaxPointLights = 8u;

            static const std::string vertex =
                "#version 120\n" \
                "#define MAX_POINT_LIGHTS 8\n" \

                "uniform vec3 u_pointLightPositions[MAX_POINT_LIGHTS] = vec3[MAX_POINT_LIGHTS]\n" \
                "(\n" \
                "    vec3(960.0, 540.0, 1300.0),\n" \
                "    vec3(0.0),\n" \
                "    vec3(0.0),\n" \
                "    vec3(0.0),\n" \
                "    vec3(0.0),\n" \
                "    vec3(0.0),\n" \
                "    vec3(0.0),\n" \
                "    vec3(0.0)\n" \
                ");\n" \
                "uniform vec3 u_cameraWorldPosition = vec3(960.0, 540.0, 480.0);\n" \
                "uniform mat4 u_inverseWorldViewMatrix;\n" \

                "varying vec3 v_eyeDirection;\n" \
                "varying vec3 v_pointLightDirections[MAX_POINT_LIGHTS];\n" \

                "const vec3 tangent = vec3(1.0, 0.0, 0.0);\n" \
                "const vec3 normal = vec3(0.0, 0.0, 1.0);\n" \

                "void main()\n" \
                "{\n" \
                "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n" \
                "    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n" \
                "    gl_FrontColor = gl_Color;\n" \

                "    mat3 normalMatrix = transpose(mat3(u_inverseWorldViewMatrix));\n" \
                "    vec3 n = normalize(normalMatrix * normal);\n" \
                "    vec3 t = normalize(normalMatrix * tangent);\n" \
                "    vec3 b = cross(n,t);\n" \
                "    mat3 tangentSpaceTransformMatrix = mat3(t.x, b.x, n.x, t.y, b.y, n.y, t.z, b.z, n.z);\n" \

                "    vec3 viewVertex = vec3(gl_ModelViewMatrix * gl_Vertex);\n" \
                "    for(int i = 0; i < MAX_POINT_LIGHTS; ++i)\n" \
                "    {\n" \
                "        vec3 viewLightDirection = vec3(gl_ModelViewMatrix * vec4(u_pointLightPositions[i], 1.0)) - viewVertex;\n"
                "        v_pointLightDirections[i] = tangentSpaceTransformMatrix * normalize(viewLightDirection);\n"
                "    }\n" \

                "    v_eyeDirection = tangentSpaceTransformMatrix * ((gl_ModelViewMatrix * vec4(u_cameraWorldPosition, 1.0)).xyz - viewVertex);\n" \
                "}";

            static const std::string fragment =
                "#version 120\n" \
                "#define MAX_POINT_LIGHTS 8\n" \
                "#define TEXTURED\n" \
                "#define SPECULAR\n" \

                "struct PointLight\n" \
                "{\n" \
                "    vec4 diffuseColour;\n" \
                "    vec4 specularColour;\n" \
                "    float inverseRange;\n" \
                "    float intensity;\n" \
                "};\n" \

                "#if defined(TEXTURED)\n" \
                "uniform sampler2D u_diffuseMap;\n" \
                "#endif\n" \
                "uniform sampler2D u_normalMap;\n" \
                "uniform vec3 u_ambientColour = vec3 (0.4, 0.4, 0.4);\n" \

                "uniform PointLight u_pointLights[MAX_POINT_LIGHTS] = PointLight[MAX_POINT_LIGHTS]\n" \
                "(\n" \
                "    PointLight(vec4(1.0), vec4(1.0), 0.5, 1.0),\n" \
                "    PointLight(vec4(1.0), vec4(1.0), 0.005, 0.0),\n" \
                "    PointLight(vec4(1.0), vec4(1.0), 0.005, 0.0),\n" \
                "    PointLight(vec4(1.0), vec4(1.0), 0.005, 0.0),\n" \
                "    PointLight(vec4(1.0), vec4(1.0), 0.005, 0.0),\n" \
                "    PointLight(vec4(1.0), vec4(1.0), 0.005, 0.0),\n" \
                "    PointLight(vec4(1.0), vec4(1.0), 0.005, 0.0),\n" \
                "    PointLight(vec4(1.0), vec4(1.0), 0.005, 0.0)\n" \
                ");\n" \

                "varying vec3 v_eyeDirection;\n" \
                "varying vec3 v_pointLightDirections[MAX_POINT_LIGHTS];\n" \

                "vec4 diffuseColour;\n" \
                "vec3 calcLighting(vec3 normal, vec3 lightDirection, vec3 lightDiffuse, vec3 lightSpec, float falloff)\n" \
                "{\n" \
                "    float diffuseAmount = max(dot(normal, lightDirection), 0.0);\n" \
                "    diffuseAmount = pow((diffuseAmount * 0.5) + 0.5, 2.0);\n" \
                "    vec3 mixedColour = lightDiffuse * diffuseColour.rgb * diffuseAmount * falloff;\n" \

                /*Blinn-Phong specular calc - TODO calc specular based on some amount - probably from material settings*/
                "#if defined(SPECULAR)\n" \
                "    vec3 eyeDirection = normalize(v_eyeDirection);\n" \
                "    vec3 halfVec = normalize(lightDirection + eyeDirection);\n" \
                "    float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);\n" \
                /*TODO switch const exponent for variable*/
                "    vec3 specularColour = lightSpec * vec3(pow(clamp(specularAngle, 0.0, 1.0), 255.0)) * falloff;\n" \
                /*TODO multiply by specular colour*/
                "    return mixedColour + (specularColour/* * SPEC_AMOUNT*/);\n" \
                "#else\n" \
                "    return mixedColour;\n" \
                "#endif\n" \
                "}\n" \

                "void main()\n" \
                "{\n" \
                "#if defined(TEXTURED)\n" \
                "    diffuseColour = texture2D(u_diffuseMap, gl_TexCoord[0].xy) * gl_Color;\n" \
                "#elif defined(COLOURED)\n" \
                "    diffuseColour = gl_Color;\n" \
                "#endif\n" \
                "    vec3 normalVector = texture2D(u_normalMap, gl_TexCoord[0].xy).rgb * 2.0 - 1.0;\n" \

                "    vec3 blendedColour = diffuseColour.rgb * u_ambientColour;\n" \
                "    for(int i = 0; i < MAX_POINT_LIGHTS; ++i)\n" \
                "    {\n" \
                "        vec3 pointLightDir = v_pointLightDirections[i] * u_pointLights[i].inverseRange;\n" \
                "        float falloff = clamp(1.0 - sqrt(dot(pointLightDir, pointLightDir)), 0.0, 1.0);\n" \
                "        blendedColour += calcLighting(normalVector, normalize(v_pointLightDirections[i]), u_pointLights[i].diffuseColour.rgb, u_pointLights[i].specularColour.rgb, falloff) * u_pointLights[i].intensity;\n" \
                "    }\n" \

                "    gl_FragColor.rgb = blendedColour;\n" \
                "    gl_FragColor.a = diffuseColour.a;\n" \
                "}";
        }
    }//namespace Shader
}//namespace xy

#endif //XY_SHADER_NORMALMAPPED_HPP_
