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

#include <xyginext/core/MessageBus.hpp>
#include <xyginext/core/Log.hpp>

#include "../imgui/imgui_dock.hpp"

using namespace xy;

namespace
{
    //max msg size is 128 bytes, so 128 messages
    //maximum per frame is 16384 bytes
    const std::size_t bufferSize = 16384u;
}

MessageBus::MessageBus()
    : m_currentBuffer   (bufferSize),
    m_pendingBuffer     (bufferSize),
    m_inPointer         (m_pendingBuffer.data()),
    m_outPointer        (m_currentBuffer.data()),
    m_currentCount      (0),
    m_pendingCount      (0),
    m_enabled           (true)
{}

const Message& MessageBus::poll()
{
    static auto size = sizeof(Message);
    const Message& m = *reinterpret_cast<Message*>(m_outPointer);
    m_outPointer += (size + m.m_dataSize);
    m_currentCount--;

    return m;
}

bool MessageBus::empty()
{
    if (m_currentCount == 0)
    {
        m_currentBuffer.swap(m_pendingBuffer);
        m_inPointer = m_pendingBuffer.data();
        m_outPointer = m_currentBuffer.data();
        m_currentCount = m_pendingCount;
        m_pendingCount = 0;
        return true;
    }
    return false;
}

std::size_t MessageBus::pendingMessageCount() const
{
    return m_pendingCount;
}

// Editor
void MessageBus::editorDraw()
{
    if (ImGui::BeginDock(("Message Bus##" + std::to_string(m_id)).c_str()))
    {
        ImGui::Text(("Message count: " + std::to_string(m_currentCount)).c_str());
        ImGui::Text(("Pending count: " + std::to_string(m_pendingCount)).c_str());
        
        ImGui::Checkbox("Enabled", &m_enabled);
    }
    ImGui::EndDock();
}
