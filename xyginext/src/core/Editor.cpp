/*********************************************************************
 (c) Jonny Paton 2017 - 2018
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

#include "xyginext/core/Editor.hpp"
#include "xyginext/core/editor/SpriteEditor.hpp"
#include "xyginext/core/editor/SceneEditor.hpp"
#include "xyginext/core/editor/IconFontAwesome5.hpp"
#include "xyginext/core/Log.hpp"
#include "xyginext/core/App.hpp"
#include "xyginext/core/ConfigFile.hpp"
#include "xyginext/graphics/SpriteSheet.hpp"
#include "xyginext/gui/Gui.hpp"
#include "xyginext/ecs/Scene.hpp"
#include "xyginext/resources/Resource.hpp"
#include "xyginext/ecs/components/ParticleEmitter.hpp"
#include "xyginext/ecs/components/Drawable.hpp"
#include "xyginext/ecs/components/Sprite.hpp"
#include "xyginext/ecs/components/Transform.hpp"
#include "xyginext/ecs/systems/RenderSystem.hpp"
#include "xyginext/ecs/systems/SpriteSystem.hpp"
#include "xyginext/ecs/systems/CameraSystem.hpp"

#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Window/WindowStyle.hpp>
#include <SFML/Window/Event.hpp>

#include "../imgui/imgui_dock.hpp"
#include "../imgui/imgui-SFML.h"
#include "../imgui/imgui_internal.h"
#include "../imgui/fonts/fa-solid-900.hpp"

#include <map>
#include <set>

using namespace xy;

namespace
{
    bool enabled = false;
    
    // bc name clash with function and bc lazy...
    bool shouldShowStyleEditor = false;
    bool shouldShowVideoSettings = false;
    bool shouldShowAudioSettings = false;
    bool shouldShowConsole = false;
    bool shouldShowAssetBrowser = false;
    bool shouldShowSceneEditor = false;
    bool shouldOpenNewPopup = false;
    bool shouldShowSettings = false;
    sf::RenderTexture viewportBuffer;
    int currentResolution = 0;
    std::array<char, 300> resolutionNames{};
    std::vector<sf::Vector2u> resolutions;
    bool fullScreen = false;
    bool vSync = false;
    bool useFrameLimit = false;
    int frameLimit = 10;
    int snapInterval = 8; // value used when selecting tex rects etc.
    
    TextureResource textureResource;
    
    // Any scenes which have the EditorSystem added
    std::unordered_map<std::string, SceneAsset> editableScenes;
    static int sceneCounter(0);
    std::string selectedSceneName = "Select a scene";
    Scene*  selectedScene = nullptr;
    
    // Asset lists
    std::map<std::string, SpriteSheetAsset> spriteSheets;
    std::map<std::string, ParticleEmitter> particleEmitters;
    std::map<std::string, std::unique_ptr<sf::Texture>> textures;
    std::map<std::string, sf::Font> fonts;
    std::map<std::string, std::fstream> textFiles;
    std::set<std::string> tmxFiles;
    std::map<std::string, std::unique_ptr<sf::SoundBuffer>> sounds;
    
    // bc we need to manage these ourselves...
    sf::Cursor moveCursor;
    sf::Cursor sizeTLBRCursor;
    sf::Cursor sizeTRBLCursor;
    sf::Cursor sizeXCursor;
    sf::Cursor sizeYCursor;
    sf::Cursor arrowCursor;
}

EditorSystem::EditorSystem(xy::MessageBus& mb, const std::string& sceneName) :
xy::System(mb, typeid(EditorSystem)),
m_sceneName(sceneName)
{
    //requireComponent<Editable>();
}

void EditorSystem::onCreate()
{
    // Register the scene we've been added to with the editor
    if (m_sceneName.empty())
    {
        m_sceneName = "Unnamed scene " + std::to_string(sceneCounter++);
    }
    
    // We store the camera, as the editor may fiddle with it, we can restore later
    // This will definitely cause a problem if the user is calling getActiveCamera() regularly
    // instead of caching it...
    editableScenes[m_sceneName].scene = getScene();
    editableScenes[m_sceneName].camera = getScene()->getActiveCamera();
}

EditorSystem::~EditorSystem()
{
    auto scene = getScene();
    auto me = std::find_if(editableScenes.begin(),editableScenes.end(),[scene](const std::pair<std::string, SceneAsset>& m)
                           { return m.second.scene == scene; });
    
    if (me != editableScenes.end())
    {
        editableScenes.erase(me);
    }
}

void Editor::init()
{
    // Merge in fontawesome icons
    auto& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF( fa_solid_900_ttf, fa_solid_900_ttf_len,  16.0f, &icons_config, icons_ranges );
    ImGui::SFML::UpdateFontTexture();
    
    // load cursors
    moveCursor.loadFromSystem(sf::Cursor::SizeAll);
    sizeTLBRCursor.loadFromSystem(sf::Cursor::SizeTopLeftBottomRight);
    sizeTRBLCursor.loadFromSystem(sf::Cursor::SizeBottomLeftTopRight);
    sizeXCursor.loadFromSystem(sf::Cursor::SizeHorizontal);
    sizeYCursor.loadFromSystem(sf::Cursor::SizeVertical);
    arrowCursor.loadFromSystem(sf::Cursor::Arrow);
    
    
    // Load all the video modes
    auto modes = sf::VideoMode::getFullscreenModes();
    for (const auto& mode : modes)
    {
        if (mode.bitsPerPixel == 32)
        {
            resolutions.emplace_back(mode.width, mode.height);
        }
    }
    
    std::reverse(std::begin(resolutions), std::end(resolutions));
    
    auto rwSize = App::getActiveInstance()->m_renderWindow.getSize();
    
    int i = 0;
    for (auto r = resolutions.begin(); r != resolutions.end(); ++r)
    {
        std::string width = std::to_string(r->x);
        std::string height = std::to_string(r->y);
        
        for (char c : width)
        {
            resolutionNames[i++] = c;
        }
        resolutionNames[i++] = ' ';
        resolutionNames[i++] = 'x';
        resolutionNames[i++] = ' ';
        for (char c : height)
        {
            resolutionNames[i++] = c;
        }
        resolutionNames[i++] = '\0';
        
        if (r->x == rwSize.x && r->y == rwSize.y)
            currentResolution = std::distance(resolutions.begin(),r);
    }
    
    auto settings = App::getActiveInstance()->getVideoSettings();
    fullScreen = (settings.WindowStyle & sf::Style::Fullscreen) == sf::Style::Fullscreen;
    vSync = settings.VSync;
    
    // Load any assets
    std::function<void(std::string)> assetSearch = [&](std::string path)
    {
        // Search subdirectories
        for (auto& dir : xy::FileSystem::listDirectories(path))
        {
            assetSearch(path + "/" + dir);
        }
        
        // Save assets
        for (auto& file : xy::FileSystem::listFiles(path))
        {
            auto ext = FileSystem::getFileExtension(file);
            auto filePath = path + "/" + file;
            
            // Would prefer a better way to handle file types - enum?
            if (ext == ".spt")
            {
                spriteSheets[file].sheet.loadFromFile(filePath, textureResource);
                spriteSheets[file].absPath = path + file;
            }
            else if (ext == ".xyp")
            {
                particleEmitters[file].settings.loadFromFile(filePath, textureResource);
            }
            else if (ext == ".png")
            {
                textures[file] = std::make_unique<sf::Texture>();
                textures[file]->loadFromFile(filePath);
            }
            else if (ext == ".ttf")
            {
                fonts[file].loadFromFile(filePath);
            }
            else if (ext == ".txt")
            {
                textFiles[file].open(filePath);
            }
            else if (ext == ".tmx")
            {
                tmxFiles.emplace(filePath);
            }
            else if (ext == ".wav" || ext == ".ogg")
            {
                sounds[file] = std::make_unique<sf::SoundBuffer>();
                sounds[file]->loadFromFile(filePath);
            }
            else
            {
                Logger::log("Resource not recognised: " + file);
            }
            
        }
    };
    
    // Check resource path and working directory (primarily required for apple...)
    assetSearch(FileSystem::getResourcePath() + "assets");
    assetSearch("assets");
    
    // Check for imgui style
    auto stylePath = FileSystem::getConfigDirectory(App::getActiveInstance()->getApplicationName()) + "style.cfg";
    Nim::Style style;
    if (style.loadFromFile(stylePath))
    {
        Nim::setStyle(style);
    }
    
    // Check for an editor settings file
    ConfigFile editorSettings;
    if (editorSettings.loadFromFile(FileSystem::getConfigDirectory(App::getActiveInstance()->getApplicationName()) + "editor.cfg"))
    {
        ConfigProperty* p;
        if ( p = editorSettings.findProperty("assetsOpen"))
        {
            shouldShowAssetBrowser = p->getValue<bool>();
        }
        if ( p = editorSettings.findProperty("consoleOpen"))
        {
            shouldShowConsole = p->getValue<bool>();
        }
        if ( p = editorSettings.findProperty("scenesOpen"))
        {
            shouldShowSceneEditor = p->getValue<bool>();
        }
        if ( p = editorSettings.findProperty("styleOpen"))
        {
            shouldShowStyleEditor = p->getValue<bool>();
        }
        if ( p = editorSettings.findProperty("audioOpen"))
        {
            shouldShowAudioSettings = p->getValue<bool>();
        }
        if ( p = editorSettings.findProperty("videoOpen"))
        {
            shouldShowVideoSettings = p->getValue<bool>();
        }
        if ( p = editorSettings.findProperty("settingsOpen"))
        {
            shouldShowSettings = p->getValue<bool>();
        }
        if ( p = editorSettings.findProperty("snap"))
        {
            snapInterval = p->getValue<int>();
        }
        
        // Check for any open spritesheets
        ConfigObject* o;
        if (o = editorSettings.findObjectWithName("OpenSpriteSheets"))
        {
            for (auto p : o->getProperties())
            {
                auto name = p.getValue<std::string>();
                spriteSheets[name].open = true;
            }
        }
    }
}

void Editor::shutdown()
{
    // Only really required bc I'm being lazy and using global sound resources
    for (auto& s : sounds)
    {
        s.second.release();
    }
    
    // Save the editor config
    // Check for an editor settings file
    ConfigFile editorSettings;
    editorSettings.addProperty("assetsOpen").setValue(shouldShowAssetBrowser);
    editorSettings.addProperty("consoleOpen").setValue(shouldShowConsole);
    editorSettings.addProperty("scenesOpen").setValue(shouldShowSceneEditor);
    editorSettings.addProperty("styleOpen").setValue(shouldShowStyleEditor);
    editorSettings.addProperty("audioOpen").setValue(shouldShowAudioSettings);
    editorSettings.addProperty("videoOpen").setValue(shouldShowVideoSettings);
    editorSettings.addProperty("settingsOpen").setValue(shouldShowSettings);
    editorSettings.addProperty("snap").setValue(snapInterval);
    
    // store any open spritesheets
    auto o = editorSettings.addObject("OpenSpriteSheets");
    for (auto& ss : spriteSheets)
    {
        if (ss.second.open)
        {
            // config item array required?
            o->addProperty("name").setValue(ss.first);
        }
    }
    
    // Finally save
    editorSettings.save(FileSystem::getConfigDirectory(App::getActiveInstance()->getApplicationName()) + "editor.cfg");
}

void Editor::toggle()
{
    enabled = !enabled;
    xy::Logger::log(std::string("Editor ") + (enabled ? "enabled" : "disabled"));
    
    // Modify the apps rendertarget
    if (enabled)
    {
        viewportBuffer.create(App::getRenderTarget()->getSize().x, App::getRenderTarget()->getSize().y);
        App::getActiveInstance()->setRenderTarget(&viewportBuffer);
    }
    else
    {
        App::getActiveInstance()->setRenderTarget(&App::getActiveInstance()->m_renderWindow);
    }
}

bool Editor::isEnabled()
{
    return enabled;
}

void Editor::draw()
{
    if (enabled)
    {
        // Probably shouldn't hardcode this
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        
        // Main menu bar first.
        float menuBarHeight = 0;
        if (ImGui::BeginMainMenuBar())
        {
            
            menuBarHeight = ImGui::GetWindowHeight();
            
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Quit","ctrl + q"))
                {
                    App::quit();
                }
                
                if (ImGui::MenuItem("New Asset", "ctrl + n"))
                {
                    shouldOpenNewPopup = true;
                }
                
                ImGui::EndMenu();
            }
            
            
            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::BeginMenu("Windows"))
                {
                    ImGui::MenuItem("Style", "", &shouldShowStyleEditor);
                    ImGui::MenuItem("Video", "", &shouldShowVideoSettings);
                    ImGui::MenuItem("Audio", "", &shouldShowAudioSettings);
                    ImGui::MenuItem("Console", "ctrl + c", &shouldShowConsole);
                    ImGui::MenuItem("Assets", "ctrl + a", &shouldShowAssetBrowser);
                    ImGui::MenuItem("Scenes", "", &shouldShowSceneEditor);
                    
                    ImGui::Separator();
                    ImGui::MenuItem("Editor settings","",&shouldShowSettings);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            
            
            ImGui::EndMainMenuBar();
        }
        
        // Create an imgui window covering the entire renderwindow (minus the menu bar)
        auto rwSize = App::getActiveInstance()->m_renderWindow.getSize();
        ImGui::SetNextWindowSize({static_cast<float>(rwSize.x), rwSize.y - menuBarHeight});
        ImGui::SetNextWindowPos({0,menuBarHeight});
        
        ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar );
        ImGui::BeginDockspace();
        
        // Show viewport
        if (ImGui::BeginDock("Viewport", nullptr))
        {
            // Make the viewport scale to available space
            auto vpw = viewportBuffer.getSize().x;
            auto vph = viewportBuffer.getSize().y;
            auto availablew = ImGui::GetContentRegionAvailWidth();
            auto availableh = ImGui::GetContentRegionAvail().y;
            auto scale = std::min(availablew / vpw, availableh / vph);
            auto size = static_cast<sf::Vector2f>(viewportBuffer.getSize()) * scale;
            ImGui::Image(viewportBuffer, size);
        }
        ImGui::EndDock();
        
        // Asset browser
        if (shouldShowAssetBrowser)
        {
            showAssetBrowser();
        }
        
        // Style editor
        if (shouldShowStyleEditor)
        {
            showStyleEditor();
        }
        
        // Video settings
        if (shouldShowVideoSettings)
        {
            showVideoSettings();
        }
        
        // Audio settings
        if (shouldShowAudioSettings)
        {
            showAudioSettings();
        }
        
        // Console
        if (shouldShowConsole)
        {
            Console::draw();
        }
        
        // Scene editor
        if (shouldShowSceneEditor)
        {
            showSceneEditor();
        }
        
        // Editor settings
        if (shouldShowSettings)
        {
            showSettings();
        }
        
        // Show any open modals
        showModalPopups();
        
        // Show any sprites open for editing
        showSpriteEditor();
        
        ImGui::EndDockspace();
        ImGui::End(); // Editor
        
        ImGui::PopFont();
    }
    
    // Check for cursor type changes here
    switch (ImGui::GetMouseCursor())
    {
        case ImGuiMouseCursor_Move:
        {
            App::getActiveInstance()->m_renderWindow.setMouseCursor(moveCursor);
            break;
        }
        case ImGuiMouseCursor_ResizeNESW:
        {
            App::getActiveInstance()->m_renderWindow.setMouseCursor(sizeTRBLCursor);
            break;
        }
        case ImGuiMouseCursor_ResizeNWSE:
        {
            App::getActiveInstance()->m_renderWindow.setMouseCursor(sizeTLBRCursor);
            break;
        }
        case ImGuiMouseCursor_ResizeNS:
        {
            App::getActiveInstance()->m_renderWindow.setMouseCursor(sizeYCursor);
            break;
        }
        case ImGuiMouseCursor_ResizeEW:
        {
            App::getActiveInstance()->m_renderWindow.setMouseCursor(sizeXCursor);
            break;
        }
        default:
        {
            App::getActiveInstance()->m_renderWindow.setMouseCursor(arrowCursor);
            break;
        }
    }
}

bool Editor::handleEvent(sf::Event& ev)
{
    if (!enabled)
    {
        return false;
    }
    
    switch(ev.type)
    {
        case sf::Event::KeyPressed:
        {
            switch(ev.key.code)
            {
                case sf::Keyboard::A:
                {
                    if (ev.key.control)
                    {
                        shouldShowAssetBrowser = !shouldShowAssetBrowser;
                    }
                }
                break;
                case sf::Keyboard::C:
                {
                    if (ev.key.control)
                    {
                        shouldShowConsole = !shouldShowConsole;
                    }
                }
                break;
                    
                case sf::Keyboard::N:
                {
                    if (ev.key.control)
                    {
                        shouldOpenNewPopup = true;
                    }
                }
                break;
                case sf::Keyboard::Q:
                {
                    if (ev.key.control)
                    {
                        App::quit();
                    }
                }
                break;
            }
            break;
        }
    }
    
    return false;
}

void Editor::showStyleEditor()
{
    if (ImGui::BeginDock("Style Editor"))
    {
        // Path to save to
        auto savePath = FileSystem::getConfigDirectory(App::getActiveInstance()->getApplicationName()) + "style.cfg";
        
        // You can pass in a reference ImGuiStyle structure to compare to, revert to and save to (else it compares to an internally stored reference)
        auto style = Nim::getStyle();
        
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.50f);
        
        ImGui::ShowFontSelector("Fonts##Selector");
        
        // Simplified Settings
        if (ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f"))
            style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
        { bool window_border = (style.WindowBorderSize > 0.0f); if (ImGui::Checkbox("WindowBorder", &window_border)) style.WindowBorderSize = window_border ? 1.0f : 0.0f; }
        ImGui::SameLine();
        { bool frame_border = (style.FrameBorderSize > 0.0f); if (ImGui::Checkbox("FrameBorder", &frame_border)) style.FrameBorderSize = frame_border ? 1.0f : 0.0f; }
        ImGui::SameLine();
        { bool popup_border = (style.PopupBorderSize > 0.0f); if (ImGui::Checkbox("PopupBorder", &popup_border)) style.PopupBorderSize = popup_border ? 1.0f : 0.0f; }
        
        // Save/Revert button
        if (ImGui::Button("Save"))
            style.saveToFile(savePath);
        
        if (ImGui::TreeNode("Rendering"))
        {
            ImGui::Checkbox("Anti-aliased lines", &style.AntiAliasedLines);
            ImGui::Checkbox("Anti-aliased fill", &style.AntiAliasedFill);
            ImGui::PushItemWidth(100);
            ImGui::DragFloat("Curve Tessellation Tolerance", &style.CurveTessellationTol, 0.02f, 0.10f, FLT_MAX, NULL, 2.0f);
            if (style.CurveTessellationTol < 0.0f) style.CurveTessellationTol = 0.10f;
            ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f, "%.2f"); // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets). But application code could have a toggle to switch between zero and non-zero.
            ImGui::PopItemWidth();
            ImGui::TreePop();
        }
        
        if (ImGui::TreeNode("Settings"))
        {
            ImGui::SliderFloat2("WindowPadding", (float*)&style.WindowPadding, 0.0f, 20.0f, "%.0f");
            ImGui::SliderFloat("PopupRounding", &style.PopupRounding, 0.0f, 16.0f, "%.0f");
            ImGui::SliderFloat2("FramePadding", (float*)&style.FramePadding, 0.0f, 20.0f, "%.0f");
            ImGui::SliderFloat2("ItemSpacing", (float*)&style.ItemSpacing, 0.0f, 20.0f, "%.0f");
            ImGui::SliderFloat2("ItemInnerSpacing", (float*)&style.ItemInnerSpacing, 0.0f, 20.0f, "%.0f");
            ImGui::SliderFloat2("TouchExtraPadding", (float*)&style.TouchExtraPadding, 0.0f, 10.0f, "%.0f");
            ImGui::SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 30.0f, "%.0f");
            ImGui::SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 20.0f, "%.0f");
            ImGui::SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 20.0f, "%.0f");
            ImGui::Text("BorderSize");
            ImGui::SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f, 1.0f, "%.0f");
            ImGui::SliderFloat("ChildBorderSize", &style.ChildBorderSize, 0.0f, 1.0f, "%.0f");
            ImGui::SliderFloat("PopupBorderSize", &style.PopupBorderSize, 0.0f, 1.0f, "%.0f");
            ImGui::SliderFloat("FrameBorderSize", &style.FrameBorderSize, 0.0f, 1.0f, "%.0f");
            ImGui::Text("Rounding");
            ImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 14.0f, "%.0f");
            ImGui::SliderFloat("ChildRounding", &style.ChildRounding, 0.0f, 16.0f, "%.0f");
            ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f");
            ImGui::SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f, 12.0f, "%.0f");
            ImGui::SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 12.0f, "%.0f");
            ImGui::Text("Alignment");
            ImGui::SliderFloat2("WindowTitleAlign", (float*)&style.WindowTitleAlign, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat2("ButtonTextAlign", (float*)&style.ButtonTextAlign, 0.0f, 1.0f, "%.2f");
            ImGui::TreePop();
        }
        
        if (ImGui::TreeNode("Colors"))
        {
            ImGui::Text("Tip: Left-click on colored square to open color picker,\nRight-click to open edit options menu.");
            
            static ImGuiTextFilter filter;
            filter.Draw("Filter colors", 200);
            
            static ImGuiColorEditFlags alpha_flags = 0;
            ImGui::RadioButton("Opaque", &alpha_flags, 0); ImGui::SameLine();
            ImGui::RadioButton("Alpha", &alpha_flags, ImGuiColorEditFlags_AlphaPreview); ImGui::SameLine();
            ImGui::RadioButton("Both", &alpha_flags, ImGuiColorEditFlags_AlphaPreviewHalf);
            
            ImGui::BeginChild("#colors", ImVec2(0, 300), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);
            ImGui::PushItemWidth(-160);
            for (int i = 0; i < static_cast<int>(Nim::Style::Colour::Count); i++)
            {
                const char* name = ImGui::GetStyleColorName(i);
                if (!filter.PassFilter(name))
                    continue;
                ImGui::PushID(i);
                auto& c = style.colours[i];
                auto col = ImColor(c.r,c.g,c.b,c.a);
                if (ImGui::ColorEdit4("##color", (float*)&col, ImGuiColorEditFlags_AlphaBar | alpha_flags))
                {
                    c.r = col.Value.w * 255;
                    c.g = col.Value.x * 255;
                    c.b = col.Value.y * 255;
                    c.a = col.Value.z * 255;
                }
                ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                ImGui::TextUnformatted(name);
                ImGui::PopID();
            }
            ImGui::PopItemWidth();
            ImGui::EndChild();
            
            ImGui::TreePop();
        }
        
        bool fonts_opened = ImGui::TreeNode("Fonts", "Fonts (%d)", ImGui::GetIO().Fonts->Fonts.Size);
        if (fonts_opened)
        {
            ImFontAtlas* atlas = ImGui::GetIO().Fonts;
            if (ImGui::TreeNode("Atlas texture", "Atlas texture (%dx%d pixels)", atlas->TexWidth, atlas->TexHeight))
            {
                ImGui::Image(atlas->TexID, ImVec2((float)atlas->TexWidth, (float)atlas->TexHeight), ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(255,255,255,128));
                ImGui::TreePop();
            }
            ImGui::PushItemWidth(100);
            for (int i = 0; i < atlas->Fonts.Size; i++)
            {
                ImFont* font = atlas->Fonts[i];
                ImGui::PushID(font);
                bool font_details_opened = ImGui::TreeNode(font, "Font %d: \'%s\', %.2f px, %d glyphs", i, font->ConfigData ? font->ConfigData[0].Name : "", font->FontSize, font->Glyphs.Size);
                ImGui::SameLine(); if (ImGui::SmallButton("Set as default")) ImGui::GetIO().FontDefault = font;
                if (font_details_opened)
                {
                    ImGui::PushFont(font);
                    ImGui::Text("The quick brown fox jumps over the lazy dog");
                    ImGui::PopFont();
                    ImGui::DragFloat("Font scale", &font->Scale, 0.005f, 0.3f, 2.0f, "%.1f");   // Scale only this font
                    ImGui::Text("Ascent: %f, Descent: %f, Height: %f", font->Ascent, font->Descent, font->Ascent - font->Descent);
                    ImGui::Text("Fallback character: '%c' (%d)", font->FallbackChar, font->FallbackChar);
                    ImGui::Text("Texture surface: %d pixels (approx) ~ %dx%d", font->MetricsTotalSurface, (int)sqrtf((float)font->MetricsTotalSurface), (int)sqrtf((float)font->MetricsTotalSurface));
                    for (int config_i = 0; config_i < font->ConfigDataCount; config_i++)
                    {
                        ImFontConfig* cfg = &font->ConfigData[config_i];
                        ImGui::BulletText("Input %d: \'%s\', Oversample: (%d,%d), PixelSnapH: %d", config_i, cfg->Name, cfg->OversampleH, cfg->OversampleV, cfg->PixelSnapH);
                    }
                    if (ImGui::TreeNode("Glyphs", "Glyphs (%d)", font->Glyphs.Size))
                    {
                        // Display all glyphs of the fonts in separate pages of 256 characters
                        const ImFontGlyph* glyph_fallback = font->FallbackGlyph; // Forcefully/dodgily make FindGlyph() return NULL on fallback, which isn't the default behavior.
                        font->FallbackGlyph = NULL;
                        for (int base = 0; base < 0x10000; base += 256)
                        {
                            int count = 0;
                            for (int n = 0; n < 256; n++)
                                count += font->FindGlyph((ImWchar)(base + n)) ? 1 : 0;
                            if (count > 0 && ImGui::TreeNode((void*)(intptr_t)base, "U+%04X..U+%04X (%d %s)", base, base+255, count, count > 1 ? "glyphs" : "glyph"))
                            {
                                float cell_spacing = style.ItemSpacing.y;
                                ImVec2 cell_size(font->FontSize * 1, font->FontSize * 1);
                                ImVec2 base_pos = ImGui::GetCursorScreenPos();
                                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                                for (int n = 0; n < 256; n++)
                                {
                                    ImVec2 cell_p1(base_pos.x + (n % 16) * (cell_size.x + cell_spacing), base_pos.y + (n / 16) * (cell_size.y + cell_spacing));
                                    ImVec2 cell_p2(cell_p1.x + cell_size.x, cell_p1.y + cell_size.y);
                                    const ImFontGlyph* glyph = font->FindGlyph((ImWchar)(base+n));;
                                    draw_list->AddRect(cell_p1, cell_p2, glyph ? IM_COL32(255,255,255,100) : IM_COL32(255,255,255,50));
                                    font->RenderChar(draw_list, cell_size.x, cell_p1, ImGui::GetColorU32(ImGuiCol_Text), (ImWchar)(base+n)); // We use ImFont::RenderChar as a shortcut because we don't have UTF-8 conversion functions available to generate a string.
                                    if (glyph && ImGui::IsMouseHoveringRect(cell_p1, cell_p2))
                                    {
                                        ImGui::BeginTooltip();
                                        ImGui::Text("Codepoint: U+%04X", base+n);
                                        ImGui::Separator();
                                        ImGui::Text("AdvanceX: %.1f", glyph->AdvanceX);
                                        ImGui::Text("Pos: (%.2f,%.2f)->(%.2f,%.2f)", glyph->X0, glyph->Y0, glyph->X1, glyph->Y1);
                                        ImGui::Text("UV: (%.3f,%.3f)->(%.3f,%.3f)", glyph->U0, glyph->V0, glyph->U1, glyph->V1);
                                        ImGui::EndTooltip();
                                    }
                                }
                                ImGui::Dummy(ImVec2((cell_size.x + cell_spacing) * 16, (cell_size.y + cell_spacing) * 16));
                                ImGui::TreePop();
                            }
                        }
                        font->FallbackGlyph = glyph_fallback;
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            static float window_scale = 1.0f;
            ImGui::DragFloat("this window scale", &window_scale, 0.005f, 0.3f, 2.0f, "%.1f");              // scale only this window
            ImGui::DragFloat("global scale", &ImGui::GetIO().FontGlobalScale, 0.005f, 0.3f, 2.0f, "%.1f"); // scale everything
            ImGui::PopItemWidth();
            ImGui::SetWindowFontScale(window_scale);
            ImGui::TreePop();
        }
        ImGui::PopItemWidth();
        Nim::setStyle(style);
    }
    ImGui::EndDock();
}

void Editor::showVideoSettings()
{
    if (ImGui::BeginDock("Video"))
    {
        // Get the video settings
        auto settings = App::getActiveInstance()->getVideoSettings();
        
        
        ImGui::Combo("Resolution", &currentResolution, resolutionNames.data());
        
        XY_ASSERT(App::getRenderTarget(), "no valid render target");
        
        ImGui::Checkbox("Full Screen", &fullScreen);
        
        ImGui::Checkbox("V-Sync", &vSync);
        if (vSync)
        {
            useFrameLimit = false;
        }
        
        ImGui::Checkbox("Limit Framerate", &useFrameLimit);
        if (useFrameLimit)
        {
            vSync = false;
        }
        
        ImGui::SameLine();
        ImGui::PushItemWidth(80.f);
        ImGui::InputInt("Frame Rate", &frameLimit);
        ImGui::PopItemWidth();
        frameLimit = std::max(10, std::min(frameLimit, 360));
        
        if (ImGui::Button("Apply", { 50.f, 20.f }))
        {
            //apply settings
            auto settings = App::getActiveInstance()->getVideoSettings();
            settings.VideoMode.width = resolutions[currentResolution].x;
            settings.VideoMode.height = resolutions[currentResolution].y;
            settings.WindowStyle = (fullScreen) ? sf::Style::Fullscreen : sf::Style::Close;
            settings.VSync = vSync;
            settings.FrameLimit = useFrameLimit ? frameLimit : 0;
            
            App::getActiveInstance()->applyVideoSettings(settings);
        }
    }
    ImGui::EndDock();
}

void Editor::showAudioSettings()
{
    if (ImGui::BeginDock("Audio"), shouldShowAudioSettings)
    {
        ImGui::Text("NOTE: only AudioSystem sounds are affected.");
        
        static float maxVol = AudioMixer::getMasterVolume();
        ImGui::SliderFloat("Master", &maxVol, 0.f, 1.f);
        AudioMixer::setMasterVolume(maxVol);
        
        static std::array<float, AudioMixer::MaxChannels> channelVol;
        for (auto i = 0u; i < AudioMixer::MaxChannels; ++i)
        {
            channelVol[i] = AudioMixer::getVolume(i);
            ImGui::SliderFloat(AudioMixer::getLabel(i).c_str(), &channelVol[i], 0.f, 1.f);
            AudioMixer::setVolume(channelVol[i], i);
        }
    }
    ImGui::EndDock();
}

void Editor::showAssetBrowser()
{
    if (ImGui::BeginDock("Assets"))
    {
       // Spritesheets
        if (spriteSheets.size())
        {
            if (ImGui::TreeNode("SpriteSheets"))
            {
                for (auto& ss : spriteSheets)
                {
                    ImGui::Selectable(ss.first.c_str(), &ss.second.open);
                }
                ImGui::TreePop();
            }
        }
        
        // Textures
        if (textures.size())
        {
            if (ImGui::TreeNode("Textures"))
            {
                for (auto& t : textures)
                {
                    if (ImGui::TreeNode(t.first.c_str()))
                    {
                        ImGui::Image(*t.second);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
        
    }
    ImGui::EndDock();
}

void Editor::showSpriteEditor()
{
    for (auto& ss : spriteSheets)
    {
        if (ss.second.open)
        {
            auto name = ss.first;
            if (ss.second.dirty)
            {
                // This causes imgui funniness, as it think's its a new dock (because different ID)
                // should be fixable in the imgui code - the previous tab stuff had an implementation for it
               // name += " *";
            }
            if (ImGui::BeginDock(name.c_str()))
            {
                // Select which texture to use
                auto file = FileSystem::getFileName(ss.second.sheet.getTexturePath());
                std::string texName =  file.length() > 0 ? file : "Select a texture";
                if (ImGui::BeginCombo("Texture", texName.c_str()))
                {
                    for (auto& tex : textures)
                    {
                        if (ImGui::Selectable(tex.first.c_str()))
                        {
                            texName = tex.first;
                            ss.second.sheet.setTexturePath("assets/" + tex.first);
                            ss.second.dirty = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                
                // triple meh
                if (texName != "Select a texture")
                {
                    
                    // Select sprite to edit
                    auto sprites = ss.second.sheet.getSprites();
                    static std::string selectedSprite = "Select a sprite";
                    if (ImGui::BeginCombo("Sprites", selectedSprite.c_str()))
                    {
                        for (auto& spr : sprites)
                        {
                            if (ImGui::Selectable(spr.first.c_str()))
                            {
                                selectedSprite = spr.first;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    
                    // Add/delete sprite
                    if (ImGui::Button("+##sprite"))
                    {
                        ss.second.sheet.setSprite("New Sprite", Sprite(*textures[texName]));
                        selectedSprite = "New Sprite";
                        ss.second.dirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("-##sprite"))
                    {
                        ss.second.sheet.removeSprite(selectedSprite);
                        selectedSprite = "Select a sprite";
                        ss.second.dirty = true;
                    }
                    
                    // meh...
                    if (selectedSprite != "Select a sprite")
                    {
                        // sprite name
                        static std::array<char, MAX_INPUT> input = {{0}};
                        selectedSprite.copy(input.data(), selectedSprite.length());
                        
                        auto spr = ss.second.sheet.getSprite(selectedSprite);
                        
                        if (ImGui::InputText("Name", input.data(), MAX_INPUT))
                        {
                            // much inefficient
                            ss.second.sheet.removeSprite(selectedSprite);
                            selectedSprite = std::string(input.data());
                            ss.second.sheet.setSprite(selectedSprite, spr);
                            ss.second.dirty = true;
                        }
                        
                        ImGui::Separator();
                        
                        SpriteEditor::editSprite(spr);
                        
                        // Animation combo list
                        static std::string selectedAnim = "Select an animation";
                        if (ImGui::BeginCombo("Animations", selectedAnim.c_str()))
                        {
                            for (auto& anim : spr.getAnimations())
                            {
                                bool animSelected(false);
                                ImGui::Selectable(anim.id.data(), &animSelected);
                                if (animSelected)
                                {
                                    selectedAnim = anim.id.data();
                                }
                            }
                            ImGui::EndCombo();
                        }
                        
                        // Add/Remove animations
                        if (ImGui::Button("+##anim"))
                        {
                            spr.getAnimations()[spr.getAnimationCount()] = Sprite::Animation();
                            selectedAnim = "New Anim";
                            selectedAnim.copy(spr.getAnimations()[spr.getAnimationCount()].id.data(), selectedAnim.length());
                            spr.setAnimationCount(spr.getAnimationCount()+1);
                            ss.second.sheet.setSprite(selectedSprite, spr);
                            ss.second.dirty = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("-##anim"))
                        {
                            auto& anims = spr.getAnimations();
                            for (auto i = 0; i < spr.getAnimationCount(); i++)
                            {
                                if (std::string(anims[i].id.data()) == selectedAnim)
                                {
                                    // move all anims up one
                                    for (auto j = i+1; j <= spr.getAnimationCount(); j++, i++)
                                    {
                                        anims[i] = anims[j];
                                    }
                                    break;
                                }
                            }
                            ss.second.sheet.setSprite(selectedSprite, spr);
                            ss.second.dirty = true;
                        }
                        
                        // double meh...
                        if (selectedAnim != "Select an animation")
                        {
                         
                            for (auto i = 0 ; i < spr.getAnimationCount(); i++)
                            {
                                auto& anim = spr.getAnimations()[i];
                                if (std::string(anim.id.data()) == selectedAnim)
                                {
                                    // Name
                                    if (ImGui::InputText("Name##anim", anim.id.data(), Sprite::Animation::MaxAnimationIdLength))
                                    {
                                        selectedAnim = anim.id.data();
                                    }
                                    
                                    // Properties
                                    ImGui::InputFloat("Framerate", &anim.framerate);
                                    ImGui::Checkbox("Loop", &anim.looped);
                                    
                                    // Frames
                                    int fc = anim.frameCount;
                                    if (ImGui::InputInt("Frame count", &fc, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue))
                                    {
                                        anim.frameCount = fc;
                                    }
                                    static int currentFrame(0);
                                    ImGui::SliderInt("Frames", &currentFrame, 0, anim.frameCount);
                                    
                                    // Tex rect of current frame
                                    auto& frame = anim.frames[currentFrame];
                                    auto texRect = static_cast<sf::IntRect>(frame);
                                    if (ImGui::InputInt4("Texture Rect##anim", (int*)&texRect))
                                    {
                                        frame = (static_cast<sf::FloatRect>(texRect));
                                    }
                                }
                            }
                        }
                        
                        // should restructure to avoid this
                        ss.second.sheet.setSprite(selectedSprite, spr);
                    }
                    
                    // Save button
                    if (ImGui::Button("Save"))
                    {
                        //not good
                        ss.second.sheet.saveToFile("assets/" + ss.first);
                    }
                    
                    // Add to scene
                    ImGui::Separator();
                    static std::string selectedScene("Select a scene");
                    if (ImGui::BeginCombo("Scene", selectedScene.c_str()))
                    {
                        for (auto& scene : editableScenes)
                        {
                            if (ImGui::Selectable(scene.first.c_str()))
                            {
                                selectedScene = scene.first;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::Button("Add to scene"))
                    {
                        auto scene = editableScenes[selectedScene];
                        
                        // Make sure the scene has the required systems
                        scene.scene->addSystem<CameraSystem>(App::getActiveInstance()->getMessageBus());
                        scene.scene->addSystem<RenderSystem>(App::getActiveInstance()->getMessageBus());
                        scene.scene->addSystem<SpriteSystem>(App::getActiveInstance()->getMessageBus());
                        
                        auto ent = scene.scene->createEntity();
                        ent.addComponent<Drawable>();
                        ent.addComponent<Transform>();
                        ent.addComponent(ss.second.sheet.getSprite(selectedSprite));
                    }
                }
            }
            ImGui::EndDock();
        }
    }
}

void Editor::showSceneEditor()
{
    if (ImGui::BeginDock("Scenes"))
    {
        // Show combo box to select scene first
        if (ImGui::BeginCombo("Scenes", selectedSceneName.c_str()))
        {
            for (auto& s : editableScenes)
            {
                if (ImGui::Selectable(s.first.c_str()))
                {
                    selectedSceneName = s.first;
                    selectedScene = s.second.scene;
                }
            }
            ImGui::EndCombo();
        }
        
        if (selectedScene)
        {
            SceneEditor::editScene(*selectedScene);
        }
        
    }
    ImGui::EndDock();
}

void Editor::showModalPopups()
{
    if (shouldOpenNewPopup)
    {
        ImGui::OpenPopup("New Asset");
        shouldOpenNewPopup = false;
    }
    
    // New Asset
    if (ImGui::BeginPopupModal("New Asset"))
    {
        static int selectedType = 0;
        
        ImGui::RadioButton("Sprite Sheet", &selectedType, 0);
        ImGui::RadioButton("Some other type...", &selectedType, 1);
        ImGui::RadioButton("Yet to be implemented...", &selectedType, 2);
        
        static std::array<char,MAX_INPUT> buf = {{0}};
        if (ImGui::InputText("Name", buf.data(), MAX_INPUT, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::Button("Create"))
        {
            switch(selectedType)
            {
                case 0: // spritesheet
                {
                    spriteSheets[buf.data()].absPath = FileSystem::getResourcePath() + buf.data();
                    spriteSheets[buf.data()].open = true;
                    spriteSheets[buf.data()].dirty = true;
                    ImGui::CloseCurrentPopup();
                    break;
                }
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void Editor::showSettings()
{
    // Snap to grid
    if (ImGui::BeginDock("Editor settings"))
    {
        ImGui::InputInt("Snap (pixels)", &snapInterval);
    }
    ImGui::EndDock();
}
