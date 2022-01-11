﻿#include "editor_imgui_windows.hpp"
#include "window.hpp"

#include "graphics/gl_context_provider.hpp"
#include "tools/tool.hpp"
#include "windows/imgui_window.hpp"

#include "erhe/imgui/imgui_impl_erhe.hpp"

#include <backends/imgui_impl_glfw.h>

namespace editor {

using namespace std;

Editor_imgui_windows::Editor_imgui_windows()
    : erhe::components::Component{c_name}
{
}

Editor_imgui_windows::~Editor_imgui_windows() = default;

void Editor_imgui_windows::connect()
{
    require<Gl_context_provider>();
    require<erhe::graphics::OpenGL_state_tracker>();
    require<Window>();
}

void Editor_imgui_windows::initialize_component()
{
    const Scoped_gl_context gl_context{Component::get<Gl_context_provider>()};

    IMGUI_CHECKVERSION();
    m_imgui_context = ImGui::CreateContext();
    ImGuiIO& io     = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
#if defined(ERHE_XR_LIBRARY_OPENXR)
    io.Fonts->AddFontFromFileTTF("res/fonts/SourceSansPro-Regular.otf", 32);
#else
    io.Fonts->AddFontFromFileTTF("res/fonts/SourceSansPro-Regular.otf", 17);
#endif

    ImFontGlyphRangesBuilder builder;

    //ImGui::Text(u8"Hiragana: 要らない.txt");
    //ImGui::Text(u8"Greek kosme κόσμε");

    // %u 8981      4E00 — 9FFF  	CJK Unified Ideographs
    // %u 3089      3040 — 309F  	Hiragana
    // %u 306A      3040 — 309F  	Hiragana
    // %u 3044.txt  3040 — 309F  	Hiragana
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());                // Basic Latin, Extended Latin
    //builder.AddRanges(io.Fonts->GetGlyphRangesKorean());                 // Default + Korean characters
    //builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());               // Default + Hiragana, Katakana, Half-Width, Selection of 2999 Ideographs
    //builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());            // Default + Half-Width + Japanese Hiragana/Katakana + full set of about 21000 CJK Unified Ideographs
    //builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());// Default + Half-Width + Japanese Hiragana/Katakana + set of 2500 CJK Unified Ideographs for common simplified Chinese
    //builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());               // Default + about 400 Cyrillic characters
    //builder.AddRanges(io.Fonts->GetGlyphRangesThai());                   // Default + Thai characters
    //builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());             // Default + Vietnamese characters

    builder.BuildRanges(&m_glyph_ranges);

    //ImGui::StyleColorsDark();
    auto* const glfw_window = reinterpret_cast<GLFWwindow*>(
        get<Window>()->get_context_window()->get_glfw_window()
    );
    ImGui_ImplGlfw_InitForOther(glfw_window, true);
    ImGui_ImplErhe_Init(get<erhe::graphics::OpenGL_state_tracker>().get());
}

void Editor_imgui_windows::menu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo"      )) {}
            if (ImGui::MenuItem("Redo"      )) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Select All")) {}
            if (ImGui::MenuItem("Deselect"  )) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Translate" )) {}
            if (ImGui::MenuItem("Rotate"    )) {}
            if (ImGui::MenuItem("Scale"     )) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Cube"  )) {}
            if (ImGui::MenuItem("Box"   )) {}
            if (ImGui::MenuItem("Sphere")) {}
            if (ImGui::MenuItem("Torus" )) {}
            if (ImGui::MenuItem("Cone"  )) {}
            if (ImGui::BeginMenu("Brush"))
            {
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Modify"))
        {
            if (ImGui::MenuItem("Ambo"    )) {}
            if (ImGui::MenuItem("Dual"    )) {}
            if (ImGui::MenuItem("Truncate")) {}
            ImGui::EndMenu();
        }
        window_menu();
        ImGui::EndMainMenuBar();
    }

    //ImGui::End();
}

void Editor_imgui_windows::window_menu()
{
    //m_frame_log_window->log("menu");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

    if (ImGui::BeginMenu("Window"))
    {
        for (auto window : m_imgui_windows)
        {
            bool enabled = window->is_visibile();
            if (ImGui::MenuItem(window->title().data(), "", &enabled))
            {
                if (enabled) {
                    window->show();
                }
                else
                {
                    window->hide();
                }
            }
        }
        ImGui::MenuItem("Tool Properties", "", &m_show_tool_properties);
        ImGui::MenuItem("ImGui Style Editor", "", &m_show_style_editor);

        ImGui::Separator();
        if (ImGui::MenuItem("Close All"))
        {
            for (auto window : m_imgui_windows)
            {
                window->hide();
            }
            m_show_tool_properties = false;
        }
        if (ImGui::MenuItem("Open All"))
        {
            for (auto window : m_imgui_windows)
            {
                window->show();
            }
            m_show_tool_properties = true;
        }
        ImGui::EndMenu();
    }

    ImGui::PopStyleVar();
}

void Editor_imgui_windows::register_imgui_window(Imgui_window* window)
{
    lock_guard<mutex> lock{m_mutex};

    m_imgui_windows.emplace_back(window);
}

void Editor_imgui_windows::imgui_windows()
{
    //Expects(m_pointer_context != nullptr);

    //const auto initial_priority_action = get_priority_action();
    //if (m_editor_view == nullptr)
    //{
    //    return;
    //}
#if !defined(ERHE_XR_LIBRARY_OPENXR)
    menu();
#endif

    for (auto imgui_window : m_imgui_windows)
    {
        if (imgui_window->is_visibile())
        {
            if (imgui_window->begin())
            {
                imgui_window->imgui();
            }
            imgui_window->end();
        }
    }

    //if (m_pointer_context->priority_action() != initial_priority_action)
    //{
    //    set_priority_action(m_pointer_context->priority_action());
    //}
    //
    //auto priority_action_tool = get_action_tool(m_pointer_context->priority_action());
    //if (
    //    m_show_tool_properties &&
    //    (priority_action_tool != nullptr)
    //)
    //{
    //    if (ImGui::Begin("Tool Properties", &m_show_tool_properties))
    //    {
    //        priority_action_tool->tool_properties();
    //    }
    //    ImGui::End();
    //}

    if (m_show_style_editor)
    {
        if (ImGui::Begin("Dear ImGui Style Editor", &m_show_style_editor))
        {
            ImGui::ShowStyleEditor();
        }
        ImGui::End();
    }
}

}  // namespace editor