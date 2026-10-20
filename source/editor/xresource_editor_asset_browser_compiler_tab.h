#ifndef ASSERT_BROWSER_COMPILER_TAB
#define ASSERT_BROWSER_COMPILER_TAB
#pragma once
#include "xresource_editor_asset_browser.h"
#include "dependencies/xeditor/include/xeditor/hint.h"
#include "xresource_editor_asset_mgr.h"
#include "xresource_editor_compile_output.h"
#include "xresource_editor_compile_view.h"
#include "imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#   define IMGUI_DEFINE_MATH_OPERATORS // Allows ImVec2 arithmetic
#endif
#include "imgui_internal.h"       // For TreeNodeBehavior, GetIDWithSeed, etc.

namespace xresource_editor
{
    //=============================================================================

    //=============================================================================

    ImVec2 CalcWrappedTextSize(const char* text, float wrap_width) {
        if (!text || wrap_width <= 0.0f) return ImVec2(0.0f, 0.0f);
        ImVec2 total_size(0.0f, 0.0f);
        const char* line_start = text;
        const ImGuiStyle& style = ImGui::GetStyle();
        float line_height = ImGui::GetTextLineHeightWithSpacing();
        int total_lines = 0;

        while (*line_start) {
            const char* line_end = line_start;
            while (*line_end && *line_end != '\n') ++line_end;

            // Calculate wrapped lines for this segment
            ImVec2 line_size = ImGui::CalcTextSize(line_start, line_end, false, wrap_width);
            int wrapped_lines = std::max(1, (int)std::ceil(line_size.x / wrap_width));
            total_size.x = std::max(total_size.x, std::min(line_size.x, wrap_width));
            total_size.y += wrapped_lines * line_height;
            total_lines += wrapped_lines;

            line_start = (*line_end == '\n') ? line_end + 1 : line_end;
        }

        // Add child window padding
        total_size.y += style.WindowPadding.y * 2;
        return total_size;
    }

    //=============================================================================

    std::string_view get_last_line(const std::string& str)
{
        if (str.empty()) return "";
        auto pos = str.rfind('\n');
        if (pos == std::string::npos) return str; // No newline, return entire string
        return std::string_view(str.data() + pos + 1, str.size() - (pos + 1)); // View from last newline to end
    }

    //=============================================================================
    inline
    float ComputeWidthComboBox(std::span< const char* const> items)
    {
        float max_width = 0.0f;
        for (const char* item : items)
        {
            ImVec2 text_size = ImGui::CalcTextSize(item);
            max_width = std::max(max_width, text_size.x);
        }

        // Add padding for dropdown arrow and frame
        max_width += ImGui::GetStyle().FramePadding.x * 2 + 20.0f; // Adjust 20.0f for arrow

        return max_width;
    }

    //=============================================================================

    [[nodiscard]] std::string ConvertHexEscapes(const std::string_view input) noexcept
    {
        constexpr static auto hex_lookup = []()consteval
            {
                std::array<std::uint8_t, 256> arr{};
                for (auto& val : arr) val = 255; // Invalid by default
                for (char c = '0'; c <= '9'; ++c) arr[c] = c - '0';
                for (char c = 'a'; c <= 'f'; ++c) arr[c] = 10 + c - 'a';
                for (char c = 'A'; c <= 'F'; ++c) arr[c] = 10 + c - 'A';
                return arr;
            }();

        std::string result;

        // Pre-allocate based on input size
        result.reserve(input.size());

        for (size_t i = 0; i < input.size();)
        {
            if (i + 3 < input.size() && input[i] == '\\' && (input[i + 1] == 'x' || input[i + 1] == 'X'))
            {
                const auto high = hex_lookup[input[i + 2]];
                const auto low = hex_lookup[input[i + 3]];
                if (high < 16 && low < 16)
                { // Valid hex pair
                    result.push_back((high << 4) | low);
                    i += 4;
                }
                else
                {
                    // Literal '\'
                    result.push_back(input[i]);
                    ++i;
                }
            }
            else
            {
                result.push_back(input[i]);
                ++i;
            }
        }
        return result;
    }

    //=============================================================================

    using namespace compile_ui;

    struct compiler_tab : xresource_editor::asset_browser_tab_base
    {
        xresource_editor::library_mgr& m_AssetMgr;

        //=============================================================================

        compiler_tab(asset_browser& Browser, const char* pName)
            : asset_browser_tab_base{ Browser, pName }
            , m_AssetMgr{ *Browser.getAssetMgr() }
        {
            m_AssetMgr.m_OnOpenProjectEvent.Register<&xresource_editor::compiler_tab::OnOpenNewProject>(*this);
            m_AssetMgr.m_OnCloseProjectEvent.Register<&xresource_editor::compiler_tab::OnCloseProject>(*this);

            if (m_AssetMgr.m_ProjectGUID.isValid())
            {
            }
        }

        //=============================================================================

        void OnOpenNewProject(xresource_editor::library_mgr&)
        {
        }

        //=============================================================================

        void OnCloseProject(xresource_editor::library_mgr&)
        {
        }

        //=============================================================================

        void LeftPanel() override
        {
            
        }


        //=============================================================================

        struct node_ui
        {
            bool    m_bExpanded = false;
            bool    m_bTabChosen = false;       // the tab that opens first (Problems when there are some) was chosen
        };
        std::unordered_map<const void*, node_ui> m_NodeUI;

        // The name and kind of a resource, and its icon (the same lookups the asset views use)
        struct node_label
        {
            std::string         m_Name;
            std::string_view    m_Type;
            ImTextureRef        m_Icon = {};
            ImVec2              m_UV0 = {}, m_UV1 = {};
            bool                m_bIcon = false;
            bool                m_bUnknown = false;
            std::string         m_Owner;            // a generated (virtual) resource: the name of the resource that makes it
            xresource::full_guid m_OwnerGuid = {};  // and which one it is
        };

        // The height of a card: a bar under the row while it runs, and the two sections when it is open
        static float CardHeight( bool bRunning, bool bExpanded ) noexcept { return (bRunning ? 66.0f : 48.0f) + (bExpanded ? 300.0f : 0.0f); }

        // The name and kind of a resource do not change while it compiles: they are looked up once in a while, not every frame for every card
        struct label_cache { node_label m_Label; double m_At = -1.0; };
        std::unordered_map<std::uint64_t, label_cache> m_Labels;

        const node_label& Describe( const compilation::historical_entry& E )
        {
            auto& C = m_Labels[E.m_Entry.m_FullGuid.m_Instance.m_Value];
            const double Now = ImGui::GetTime();
            if (C.m_At < 0.0 || Now - C.m_At > 3.0) { C.m_Label = DescribeNow(E); C.m_At = Now; }
            return C.m_Label;
        }

        node_label DescribeNow( const compilation::historical_entry& E )
        {
            node_label L;
            m_AssetMgr.m_mLibraryDB.FindAsReadOnly(E.m_Entry.m_gLibrary, [&](const std::unique_ptr<library_db>& Library)
            {
                Library->m_InfoByTypeDataBase.FindAsReadOnly(E.m_Entry.m_FullGuid.m_Type, [&](const std::unique_ptr<library_db::info_db>& InfoDB)
                {
                    if (auto e = m_AssetMgr.m_AssetPluginsDB.m_mPluginsByTypeGUID.find(E.m_Entry.m_FullGuid.m_Type); e != m_AssetMgr.m_AssetPluginsDB.m_mPluginsByTypeGUID.end())
                    {
                        L.m_Type = m_AssetMgr.m_AssetPluginsDB.m_lPlugins[e->second].m_TypeName;
                        if (auto Icon = m_AssetMgr.m_AssetPluginsDB.getIconRef(E.m_Entry.m_FullGuid.m_Type); Icon.isValid())
                        {
                            L.m_Icon = (ImTextureRef)(void*)Icon.m_pTexture;
                            L.m_UV0 = ImVec2(Icon.m_U0, Icon.m_V0);
                            L.m_UV1 = ImVec2(Icon.m_U1, Icon.m_V1);
                            L.m_bIcon = true;
                        }
                    }
                    if (false == InfoDB->m_InfoDataBase.FindAsReadOnly(E.m_Entry.m_FullGuid.m_Instance, [&](const library_db::info_node& InfoNode)
                    {
                        L.m_Name = InfoNode.m_Info.m_Name.empty() ? std::format("{:X}", E.m_Entry.m_FullGuid.m_Instance.m_Value) : InfoNode.m_Info.m_Name;
                    }))
                    {
                        L.m_Name = std::format("{:X} (not in the database)", E.m_Entry.m_FullGuid.m_Instance.m_Value);
                        L.m_bUnknown = true;
                    }
                });
            });
            if (L.m_Name.empty()) L.m_Name = std::format("{:X}", E.m_Entry.m_FullGuid.m_Instance.m_Value);

            // Generated by another resource (a skin makes its skeleton and animations): say whose, so a card is never a mystery
            xresource::full_guid Owner = E.m_Entry.m_FullGuid;
            m_AssetMgr.m_mLibraryDB.FindAsReadOnly(E.m_Entry.m_gLibrary, [&](const std::unique_ptr<library_db>& Library) { Owner = Library->RootOwner(E.m_Entry.m_FullGuid); });
            if (Owner != E.m_Entry.m_FullGuid)
                m_AssetMgr.getNodeInfo(E.m_Entry.m_gLibrary, Owner, [&](library_db::info_node& N) { L.m_Owner = N.m_Info.m_Name.empty() ? std::format("{:X}", Owner.m_Instance.m_Value) : N.m_Info.m_Name; L.m_OwnerGuid = Owner; });
            return L;
        }

        //=============================================================================

        // The warnings and errors of a compile, one row each: its glyph, the message, and the lines that belong to it (muted). A right click copies it.
        void RenderProblems( const compile_ui::log_view& V, const compilation::historical_entry& E )
        {
            const auto& P = *V.m_pParsed;
            const bool bFailedWithoutWords = V.m_Result == compilation::historical_entry::result::FAILURE && P.m_nErrors == 0;
            if (P.m_Problems.empty() && !bFailedWithoutWords)
            {
                const ImVec2 C = ImGui::GetCursorScreenPos();
                auto* pList = ImGui::GetWindowDrawList();
                pList->AddCircle(ImVec2(C.x + 10.0f, C.y + 10.0f), 6.5f, Fade(ColOk(), 200), 0, 1.5f);
                pList->AddLine(ImVec2(C.x + 7.0f, C.y + 10.5f), ImVec2(C.x + 9.5f, C.y + 13.0f), Fade(ColOk(), 220), 1.5f);
                pList->AddLine(ImVec2(C.x + 9.5f, C.y + 13.0f), ImVec2(C.x + 14.0f, C.y + 7.0f), Fade(ColOk(), 220), 1.5f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 26.0f);
                ImGui::TextDisabled(V.m_bRunning ? "Nothing to report so far" : "No warnings or errors");
                return;
            }

            auto Row = [&](bool bError, const std::string& Text)
            {
                const ImVec2 Start = ImGui::GetCursorScreenPos();
                const float  Wrap  = ImGui::GetContentRegionAvail().x - 28.0f;
                const auto   Nl    = Text.find('\n');
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 24.0f);
                ImGui::BeginGroup();
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + Wrap);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(bError ? ColError() : ColWarn()));
                ImGui::TextUnformatted(Text.c_str(), Nl == std::string::npos ? Text.c_str() + Text.size() : Text.c_str() + Nl);
                ImGui::PopStyleColor();
                if (Nl != std::string::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ColMuted()));
                    ImGui::TextUnformatted(Text.c_str() + Nl + 1);
                    ImGui::PopStyleColor();
                }
                ImGui::PopTextWrapPos();
                ImGui::EndGroup();
                const ImVec2 End = ImGui::GetItemRectMax();
                auto* pList = ImGui::GetWindowDrawList();
                DrawGlyph(pList, ImVec2(Start.x + 9.0f, Start.y + ImGui::GetTextLineHeight() * 0.5f + 1.0f), bError);
                pList->AddLine(ImVec2(Start.x, End.y + 3.0f), ImVec2(Start.x + ImGui::GetContentRegionAvail().x, End.y + 3.0f), IM_COL32(255, 255, 255, 14));
                if (ImGui::IsMouseHoveringRect(Start, End) && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) ImGui::SetClipboardText(Text.c_str());
                if (ImGui::IsMouseHoveringRect(Start, End) && ImGui::IsWindowHovered()) xeditor::hint::Text("Right click copies this message");
                ImGui::Dummy(ImVec2(0, 5.0f));
            };

            for (const auto& M : P.m_Problems) Row(M.m_Level == compilation::message_level::error, M.m_Text);
            if (bFailedWithoutWords) Row(true, "The compile failed. The compiler gave no message of its own: the Output tab has all it printed.");
        }

        // The compiler's own words, line by line, with the problems coloured; it follows the end of the output while the compile runs
        void RenderOutput( const compile_ui::log_view& V )
        {
            if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(V.m_Raw.c_str());
            if (ImGui::IsItemHovered()) xeditor::hint::Text("Copy everything the compiler printed");
            ImGui::SameLine();
            ImGui::TextDisabled("%zu bytes", V.m_Raw.size());

            std::vector<std::string_view> Lines;
            for (std::size_t Start = 0; Start <= V.m_Raw.size(); )
            {
                const auto Eol = V.m_Raw.find('\n', Start);
                auto Line = std::string_view(V.m_Raw).substr(Start, Eol == std::string::npos ? V.m_Raw.size() - Start : Eol - Start);
                while (!Line.empty() && Line.back() == '\r') Line.remove_suffix(1);
                Lines.push_back(Line);
                if (Eol == std::string::npos) break;
                Start = Eol + 1;
            }

            ImGui::BeginChild("##raw", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            const bool bAtEnd = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f;
            ImGuiListClipper Clip;
            Clip.Begin(static_cast<int>(Lines.size()), ImGui::GetTextLineHeight());
            while (Clip.Step())
                for (int i = Clip.DisplayStart; i < Clip.DisplayEnd; ++i)
                {
                    const auto Line = Lines[static_cast<std::size_t>(i)];
                    compilation::message_level Level; std::string_view Text; float F; std::string Title;
                    ImU32 Col = IM_COL32(205, 205, 205, 255);
                    if (compilation::details::Tagged(Line, Level, Text))
                        Col = Level == compilation::message_level::error ? ColError() : Level == compilation::message_level::warning ? ColWarn() : ColMuted();
                    else if (compilation::details::Progress(Line, F, Title)) Col = ColMuted();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Col));
                    ImGui::TextUnformatted(Line.data(), Line.data() + Line.size());
                    ImGui::PopStyleColor();
                }
            if (V.m_bRunning && bAtEnd) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
        }

        //=============================================================================

        // "owner >": the name of what makes a generated resource, muted, and a small chevron, on the line of the name that follows
        static void Crumb( const char* pOwner ) noexcept
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ColMuted()));
            ImGui::TextUnformatted(pOwner);
            ImGui::PopStyleColor();
            const ImVec2 Min = ImGui::GetItemRectMin(), Max = ImGui::GetItemRectMax();
            const float  X = Max.x + 6.0f, Y = (Min.y + Max.y) * 0.5f;
            auto* pList = ImGui::GetWindowDrawList();
            pList->AddLine(ImVec2(X, Y - 4.0f), ImVec2(X + 4.0f, Y), ColMuted(), 1.5f);
            pList->AddLine(ImVec2(X + 4.0f, Y), ImVec2(X, Y + 4.0f), ColMuted(), 1.5f);
            ImGui::SameLine(0.0f, 16.0f);
        }

        // One resource of an order: a card with its name and kind, how it is going (a state, how long, its warnings and errors) and a bar; opened, it has two
        // sections: the problems (what to read) and the output (what the compiler said). Returns the rectangle of the card.
        void RenderNode( compilation::historical_entry& E, ImVec2& CardMin, bool bUnderItsOwner )
        {
            const void* Key = E.m_Log ? static_cast<const void*>(E.m_Log.get()) : static_cast<const void*>(&E);
            auto& UI = m_NodeUI[Key];
            const auto& Label = Describe(E);
            const auto V      = compile_ui::ReadLog(E.m_Log, UI.m_bExpanded);
            const char* pWord; ImU32 StateCol; bool bSpin;
            StateOf(V.m_Result, pWord, StateCol, bSpin);
            const auto& P = V.m_pParsed;
            const int nErr  = P ? P->m_nErrors   : 0;
            const int nWarn = P ? P->m_nWarnings : 0;
            const bool bBar = V.m_bRunning;

            if (Key == m_ScrollKey) { ImGui::SetScrollHereY(0.0f); m_ScrollKey = nullptr; }
            ImGui::PushID(Key);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, V.m_bRunning ? 46 : 22));
            ImGui::BeginChild("##card", ImVec2(0, CardHeight(bBar, UI.m_bExpanded)), true, ImGuiWindowFlags_NoScrollWithMouse);      // (without NoScrollbar too: the wheel over a card goes on to the list)

            // the row: open/close, the kind, the name ... the numbers
            if (ImGui::ArrowButton("##open", UI.m_bExpanded ? ImGuiDir_Down : ImGuiDir_Right)) UI.m_bExpanded = !UI.m_bExpanded;
            ImGui::SameLine();
            if (Label.m_bIcon) { ImGui::Image(Label.m_Icon, ImVec2(22, 22), Label.m_UV0, Label.m_UV1); ImGui::SameLine(); }
            ImGui::PushFont(xgpu::tools::imgui::getLargeEmphasisFont());
            // A generated resource says whose it is, as a path ("lily > Animations"): alone in the list that is the whole story; shown inside its owner's block the owner is the
            // block, and the card is just its name
            if (!Label.m_Owner.empty() && !bUnderItsOwner) { Crumb(Label.m_Owner.c_str()); }
            if (Label.m_bUnknown) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ColError()));
            ImGui::TextUnformatted(Label.m_Name.c_str());
            if (Label.m_bUnknown) ImGui::PopStyleColor();
            ImGui::PopFont();
            if (!Label.m_Type.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%.*s", static_cast<int>(Label.m_Type.size()), Label.m_Type.data()); }
            // names repeat, guids do not: the tail of it tells two of the same name apart
            ImGui::SameLine(); ImGui::TextDisabled("\xC2\xB7 %06llX", static_cast<unsigned long long>(E.m_Entry.m_FullGuid.m_Instance.m_Value & 0xFFFFFF));

            const std::string Time = V.m_Seconds < 0.0 ? std::string{} : V.m_bQueued ? "waiting " + FormatSeconds(V.m_Seconds) : FormatSeconds(V.m_Seconds);
            const float ChipW   = ImGui::CalcTextSize(pWord).x + 16.0f + (bSpin ? 16.0f : 0.0f);
            const float TimeW   = Time.empty() ? 0.0f : ImGui::CalcTextSize(Time.c_str()).x + 10.0f;
            auto BadgeW = [](int N) { return N ? 18.0f + ImGui::CalcTextSize(std::format("{}", N).c_str()).x + 8.0f : 0.0f; };
            const float RightW  = ChipW + TimeW + BadgeW(nErr) + BadgeW(nWarn) + 8.0f;
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX() + 8.0f, ImGui::GetWindowWidth() - RightW - 10.0f));
            auto Badge = [&](bool bError, int N)
            {
                if (!N) return;
                const ImVec2 C = ImGui::GetCursorScreenPos();
                DrawGlyph(ImGui::GetWindowDrawList(), ImVec2(C.x + 7.0f, C.y + ImGui::GetTextLineHeight() * 0.5f + 2.0f), bError);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(bError ? ColError() : ColWarn()));
                ImGui::Text("%d", N);
                ImGui::PopStyleColor();
                ImGui::SameLine(0.0f, 8.0f);
            };
            Badge(true, nErr);
            Badge(false, nWarn);
            Chip(pWord, StateCol, bSpin);
            if (!Time.empty()) { ImGui::SameLine(0.0f, 10.0f); ImGui::TextDisabled("%s", Time.c_str()); }

            // the bar, with what the compiler is doing
            if (bBar)
            {
                compile_ui::ProgressBar(V, ImVec2(-1.0f, 6.0f));
                if (P && P->m_bProgress) ImGui::TextDisabled("%s  %d%%", P->m_ProgressTitle.empty() ? "Working" : P->m_ProgressTitle.c_str(), static_cast<int>(P->m_Progress * 100.0f + 0.5f));
                else                     ImGui::TextDisabled("Working...");
            }

            // opened: the problems and the output
            if (UI.m_bExpanded)
            {
                ImGui::Separator();
                if (ImGui::BeginTabBar("##sections"))
                {
                    const bool bSome = nErr + nWarn > 0 || (V.m_Result == compilation::historical_entry::result::FAILURE);
                    ImGuiTabItemFlags FirstProblems = 0, FirstOutput = 0;
                    if (!UI.m_bTabChosen) { (bSome ? FirstProblems : FirstOutput) = ImGuiTabItemFlags_SetSelected; UI.m_bTabChosen = true; }

                    const std::string ProblemsTitle = nErr + nWarn ? std::format("Problems  {}", nErr + nWarn) : std::string("Problems");
                    if (ImGui::BeginTabItem((ProblemsTitle + "###problems").c_str(), nullptr, FirstProblems))
                    {
                        ImGui::BeginChild("##problems", ImVec2(0, 0), false);
                        RenderProblems(V, E);
                        ImGui::EndChild();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Output###output", nullptr, FirstOutput))
                    {
                        RenderOutput(V);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }

            ImGui::EndChild();
            CardMin = ImGui::GetItemRectMin();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::PopID();
        }

        //=============================================================================

        // One compilation order as a block: a header that says how far the whole order is (a bar over all of its resources, the problems of all of them), and
        // under it the resource and what its compile made (a font's atlas, a geometry's materials), each a card, children indented and joined to their parent.
        void RenderOrder( const std::vector<compilation::instance::orders::info>& Nodes, std::uint64_t Root, bool bOver, compilation::order_outcome Outcome )
        {
            using result = compilation::historical_entry::result;

            // A block that is out of sight is a space of the same height: a build of hundreds of resources must not cost a frame
            {
                const ImVec2 Clip0 = ImGui::GetWindowDrawList()->GetClipRectMin(), Clip1 = ImGui::GetWindowDrawList()->GetClipRectMax();
                const float  Top   = ImGui::GetCursorScreenPos().y;
                const float  Gap   = ImGui::GetStyle().ItemSpacing.y;
                float H = Nodes.size() > 1 ? m_HeaderHeight : 0.0f;       // (an order of one resource is its card alone)
                for (const auto& N : Nodes)
                {
                    bool bRunning = false;
                    if (N.m_Log) { xcontainer::lock::scope lock(*N.m_Log); const auto R = N.m_Log->get().m_Result; bRunning = R == result::COMPILING || R == result::COMPILING_WARNINGS; }
                    const auto It = m_NodeUI.find(N.m_Log ? static_cast<const void*>(N.m_Log.get()) : nullptr);
                    H += CardHeight(bRunning, It != m_NodeUI.end() && It->second.m_bExpanded) + Gap;
                }
                bool bFocus = false;
                for (const auto& N : Nodes) bFocus |= N.m_Log && N.m_Log.get() == m_ScrollKey;
                if (!bFocus && (Top + H < Clip0.y - 80.0f || Top > Clip1.y + 80.0f)) { ImGui::Dummy(ImVec2(0, H - Gap)); return; }
            }
            const float BlockTop = ImGui::GetCursorScreenPos().y;
            const bool  bHeader  = Nodes.size() > 1;
            ImGui::PushID(reinterpret_cast<void*>(static_cast<std::uintptr_t>(Root)));
            if (bHeader)
            {
                int Done = 0, nErr = 0, nWarn = 0;
                float Fraction = 0.0f;
                std::string RootName;
                double Seconds = -1.0;
                for (const auto& N : Nodes)
                {
                    if (N.m_bEnded) ++Done;
                    compilation::historical_entry E;
                    E.m_Entry.m_FullGuid = N.m_Guid; E.m_Entry.m_gLibrary = N.m_Meta; E.m_Log = N.m_Log;
                    const auto V = compile_ui::ReadLog(E.m_Log, false);
                    if (V.m_pParsed) { nErr += V.m_pParsed->m_nErrors; nWarn += V.m_pParsed->m_nWarnings; }
                    const bool bNodeDone = N.m_bEnded || V.m_Result == result::SUCCESS || V.m_Result == result::SUCCESS_WARNINGS || V.m_Result == result::FAILURE || V.m_Result == result::CANCELLED;
                    Fraction += bNodeDone ? 1.0f : (V.m_bRunning && V.m_pParsed && V.m_pParsed->m_bProgress ? V.m_pParsed->m_Progress : 0.0f);
                    if (N.m_Order == Root) { const auto& Name = Describe(E); RootName = Name.m_Owner.empty() ? Name.m_Name : Name.m_Owner + " > " + Name.m_Name; }
                    if (V.m_Seconds >= 0.0 && (V.m_bRunning || V.m_bQueued || N.m_Order == Root)) Seconds = std::max(Seconds, V.m_Seconds);
                }
                Fraction = Nodes.empty() ? 1.0f : Fraction / static_cast<float>(Nodes.size());

                bool bAnyStarted = false;
                for (const auto& N : Nodes)
                {
                    if (N.m_bEnded) { bAnyStarted = true; break; }
                    if (N.m_Log) { xcontainer::lock::scope lock(*N.m_Log); if (N.m_Log->get().m_Result != result::QUEUED) { bAnyStarted = true; break; } }
                }
                const char* pWord = !bOver ? (bAnyStarted ? "Compiling" : "Queued") : Outcome == compilation::order_outcome::failed ? "Failed" : Outcome == compilation::order_outcome::cancelled ? "Cancelled" : (nWarn ? "Done with warnings" : "Done");
                const ImU32 Col   = !bOver ? (bAnyStarted ? ColRun() : ColMuted()) : Outcome == compilation::order_outcome::failed ? ColError() : Outcome == compilation::order_outcome::cancelled ? ColMuted() : (nWarn ? ColWarn() : ColOk());

                ImGui::Dummy(ImVec2(0, 4.0f));
                // (no title and no count: the cards below say what is in the run)
                ImGui::Dummy(ImVec2(1.0f, ImGui::GetTextLineHeight()));
                {
                    auto BadgeW = [](int N) { return N ? 18.0f + ImGui::CalcTextSize(std::format("{}", N).c_str()).x + 8.0f : 0.0f; };
                    const std::string Time = Seconds >= 0.0 ? FormatSeconds(Seconds) : std::string{};
                    const float RightW = ImGui::CalcTextSize(pWord).x + 16.0f + (!bOver && bAnyStarted ? 16.0f : 0.0f) + (Time.empty() ? 0.0f : ImGui::CalcTextSize(Time.c_str()).x + 10.0f) + BadgeW(nErr) + BadgeW(nWarn) + 8.0f;
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX() + 8.0f, ImGui::GetWindowWidth() - RightW - 10.0f));
                    auto Badge = [&](bool bError, int N)
                    {
                        if (!N) return;
                        const ImVec2 C = ImGui::GetCursorScreenPos();
                        DrawGlyph(ImGui::GetWindowDrawList(), ImVec2(C.x + 7.0f, C.y + ImGui::GetTextLineHeight() * 0.5f + 2.0f), bError);
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18.0f);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(bError ? ColError() : ColWarn()));
                        ImGui::Text("%d", N);
                        ImGui::PopStyleColor();
                        ImGui::SameLine(0.0f, 8.0f);
                    };
                    Badge(true, nErr);
                    Badge(false, nWarn);
                    Chip(pWord, Col, !bOver && bAnyStarted);
                    if (!Time.empty()) { ImGui::SameLine(0.0f, 10.0f); ImGui::TextDisabled("%s", Time.c_str()); }
                }
                if (bOver)
                {
                    // finished: a plain line (a full bar would look like a progress that never ends)
                    const ImVec2 P = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(P.x, P.y + 1.0f), ImVec2(P.x + ImGui::GetContentRegionAvail().x, P.y + 1.0f), IM_COL32(255, 255, 255, 30));
                    ImGui::Dummy(ImVec2(0, 3.0f));
                }
                else
                {
                    // running: how far the whole run is
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::ColorConvertU32ToFloat4(Col));
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, 0.07f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
                    ImGui::ProgressBar(Fraction, ImVec2(-1.0f, 4.0f), "");
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(2);
                }
                ImGui::Dummy(ImVec2(0, 2.0f));
                m_HeaderHeight = ImGui::GetCursorScreenPos().y - BlockTop;          // what the header takes, as laid out: the spaces of the blocks out of sight use it
            }

            std::unordered_map<std::uint64_t, int> Depth;
            std::unordered_map<std::uint64_t, xresource::full_guid> Guids;
            for (const auto& N : Nodes)
            {
                const int D = N.m_Parent && Depth.contains(N.m_Parent) ? Depth[N.m_Parent] + 1 : 0;
                Depth[N.m_Order] = D;

                compilation::historical_entry E;
                E.m_Entry.m_FullGuid = N.m_Guid;
                E.m_Entry.m_gLibrary = N.m_Meta;
                E.m_Entry.m_Order    = N.m_Order;
                E.m_Log              = N.m_Log;

                if (D) ImGui::Indent(24.0f * D);
                ImVec2 CardMin;
                bool bUnderOwner = false;
                if (N.m_Parent)
                    if (auto P = Guids.find(N.m_Parent); P != Guids.end()) bUnderOwner = Describe(E).m_OwnerGuid == P->second;
                Guids[N.m_Order] = N.m_Guid;
                RenderNode(E, CardMin, bUnderOwner);
                if (D)
                {
                    // the branch that joins a child to what made it
                    auto* pList = ImGui::GetWindowDrawList();
                    const float X = CardMin.x - 12.0f;
                    pList->AddLine(ImVec2(X, CardMin.y - 8.0f), ImVec2(X, CardMin.y + 22.0f), IM_COL32(255, 255, 255, 40), 1.5f);
                    pList->AddLine(ImVec2(X, CardMin.y + 22.0f), ImVec2(CardMin.x - 2.0f, CardMin.y + 22.0f), IM_COL32(255, 255, 255, 40), 1.5f);
                    ImGui::Unindent(24.0f * D);
                }
            }
            ImGui::PopID();
        }

        bool  m_bOnlyProblems = false;
        const void* m_ScrollKey = nullptr;      // the card to bring into view (a resource editor's Feedback asked for it)

        // A resource editor's Feedback: the compile of that resource (the newest one) is opened, and shown. The filter must not hide it.
        void FocusNodeOf( std::uint64_t Instance )
        {
            const void* Found = nullptr;
            const auto Open = m_AssetMgr.m_Compilation.m_Orders.Snapshot();
            for (auto it = Open.rbegin(); it != Open.rend() && !Found; ++it)
                if (it->m_Guid.m_Instance.m_Value == Instance && it->m_Log) Found = it->m_Log.get();
            if (!Found)
            {
                std::lock_guard lock(m_AssetMgr.m_Compilation.m_TreesMutex);
                for (auto t = m_AssetMgr.m_Compilation.m_Trees.rbegin(); t != m_AssetMgr.m_Compilation.m_Trees.rend() && !Found; ++t)
                    for (auto n = t->m_Members.rbegin(); n != t->m_Members.rend() && !Found; ++n)
                        if (n->m_Guid.m_Instance.m_Value == Instance && n->m_Log) Found = n->m_Log.get();
            }
            if (!Found) return;
            for (auto& KV : m_NodeUI) KV.second.m_bExpanded = false;        // the one the person came to see is the only one open
            auto& UI = m_NodeUI[Found];
            UI.m_bExpanded = true;
            UI.m_bTabChosen = false;            // it opens on the problems when it has some, on the output when it has not
            m_ScrollKey = Found;
            m_bOnlyProblems = false;
        }
        float m_HeaderHeight  = 48.0f;

        // Did anything of this order have something to say (a warning, an error, or an order that did not succeed)?
        bool HasProblems( const std::vector<compilation::instance::orders::info>& Nodes, compilation::order_outcome Outcome )
        {
            if (Outcome != compilation::order_outcome::success) return true;
            for (const auto& N : Nodes)
            {
                compilation::historical_entry E;
                E.m_Log = N.m_Log;
                const auto V = compile_ui::ReadLog(E.m_Log, false);
                if (V.m_pParsed && (V.m_pParsed->m_nErrors || V.m_pParsed->m_nWarnings)) return true;
            }
            return false;
        }

        // Groups the orders by the tree they are in: the nodes of each root, oldest first
        using order_tree = std::pair<std::uint64_t, std::vector<compilation::instance::orders::info>>;
        static std::vector<order_tree> GroupByRoot( const std::vector<compilation::instance::orders::info>& All )
        {
            std::vector<order_tree> Trees;
            for (const auto& N : All)
            {
                auto It = std::ranges::find(Trees, N.m_Root, &order_tree::first);
                if (It == Trees.end()) { Trees.push_back({ N.m_Root, {} }); It = Trees.end() - 1; }
                It->second.push_back(N);
            }
            return Trees;
        }

        //=============================================================================

        void RightPanel() override
        {
            const ImVec4 normalColor = ImGui::GetStyle().Colors[ImGuiCol_Button];

            // Toolbar buttons stay transparent (flat against the header bar) - direct user correction:
            // this used to ALSO force ImGuiCol_ChildBg fully transparent, which fell through to the
            // plain window background for the whole items list + every entry's own child below,
            // making this view look flat black instead of matching the Resources/Assets views' own
            // (unmodified, theme-default) lighter panel background. Toolbar buttons above never sit
            // inside a BeginChild anyway, so dropping the ChildBg override only affects the list.
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

            //
            // Main Menu
            //
            {
                if (ImGui::Button("\xEE\x9C\x80\xee\xa5\xb2"))   // \xEE\xB7\xA3
                {
                    // Get button position and size
                    ImVec2 button_pos = ImGui::GetItemRectMin();
                    ImVec2 button_size = ImGui::GetItemRectSize();

                    // Set popup position just below the button
                    ImGui::SetNextWindowPos(ImVec2(button_pos.x, button_pos.y + button_size.y));
                    ImGui::OpenPopup("Compilation Main Menu");
                }

                if (ImGui::BeginPopup("Compilation Main Menu"))
                {
                    if (ImGui::BeginMenu("  Recompile"))
                    {
                        // Was: delete Cache/Resources/Platforms and hope a later rescan notices the
                        // compiled output is gone - it never actually re-queued anything itself. Now
                        // uses the same "clear timestamps, let AddToCompilationQueueIfNeeded decide"
                        // primitive RecompileResource already established for a single resource
                        // (xresource_editor_asset_mgr.h), just walked across every resource in every library.
                        if (ImGui::MenuItem("  All Resources"))
                            m_AssetMgr.RecompileAllResources();

                        // Was completely unimplemented (empty menu item). Requeues exactly the
                        // resources currently sitting in compilation::instance::m_Failed (populated on
                        // every FAILURE, never consumed anywhere else until now).
                        if (ImGui::MenuItem("  Errors Resources"))
                            m_AssetMgr.RecompileFailedResources();

                        ImGui::EndMenu();
                    }

                    if (ImGui::MenuItem("  Setting"))
                    {

                    }
                    ImGui::EndPopup();
                }
            }

            //
            // Auto Compile
            //
            {
                ImGui::SameLine();

                const bool  OldState = m_AssetMgr.m_Compilation.m_AutoCompilation.load();
                if (OldState) 
                {
                    ImGui::PushStyleColor(ImGuiCol_Button,          normalColor);
                }

                if (ImGui::Button(OldState ? "\xEE\xA6\x9A Compile" : "\xEE\xA6\x9A" ) ) // :  "\xEE\x9F\x89"
                {
                    m_AssetMgr.m_Compilation.m_AutoCompilation.store(!OldState);

                    // let us help star the process...
                    if (OldState==false) m_AssetMgr.m_Compilation.StartCompilation();
                }

                if (ImGui::IsItemHovered()) xeditor::hint::Text(OldState ? "Press if you want to make the compilation system be manual" : "Press if you want the compilation system to be automatic");

                if (OldState) 
                {
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::SameLine();
                    if (m_AssetMgr.m_Compilation.m_PauseCompilation.load()) ImGui::BeginDisabled();
                    if ( ImGui::Button("\xEF\x96\xB0 Compile") )
                    {
                        // Trigger compilation manually
                        m_AssetMgr.m_Compilation.StartCompilation();
                    }
                    if (ImGui::IsItemHovered()) xeditor::hint::Text("Press to run compilation process");
                    if (m_AssetMgr.m_Compilation.m_PauseCompilation.load()) ImGui::EndDisabled();
                }
            }

            //
            // Pause
            //
            {
                ImGui::SameLine();
                const bool OldState = m_AssetMgr.m_Compilation.m_PauseCompilation.load();

                if (OldState)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, normalColor);
                }

                // Was "\xEE\x98\xAE" - rendered as a plain "?" tofu box in this font build (direct user
                // report), same class of broken-codepoint issue already hit and fixed elsewhere this
                // project (see e10_asset_tree_polish_pass2 memory) - only a live screenshot proves a
                // codepoint actually renders here, never IsGlyphInFont/bbox checks. U+E769 ("Pause")/
                // U+E768 ("Play", shown once already paused, to resume) - real Segoe MDL2 Assets
                // codepoints, live-verified this pass.
                if ( ImGui::Button( OldState ? "\xEE\x9D\xA8" : "\xEE\x9D\xA9" ))
                {
                    m_AssetMgr.m_Compilation.PauseCompilation(!OldState);

                    // REAL BUG FOUND LIVE (2026-09-11): Pause correctly halts the queue (the
                    // compilation_job's own ContinueCompiling() check makes OnRun() return/exit
                    // entirely rather than wait), but flipping the flag back on Resume was never
                    // enough on its own - nothing re-submits the now-exited job to the scheduler,
                    // so queued items sat at "Waiting to Compile" forever after a resume, live-
                    // verified stuck for 7+ seconds. Same fix the Auto-Compile toggle right above
                    // already uses when re-enabling itself (StartCompilation() is a no-op if a job
                    // is already running, so this is always safe to call).
                    if (OldState) m_AssetMgr.m_Compilation.StartCompilation();
                }
                if (ImGui::IsItemHovered()) xeditor::hint::Text(OldState ? "Press if you want to resume the compilation process" : "Press to pause the compilation process");

                if (OldState)
                {
                    ImGui::PopStyleColor();
                }

            }

            //
            // Options
            //
            {
                std::scoped_lock lk(m_AssetMgr.m_Compilation.m_Settings.m_Mutex);

                // Debug Level
                {
                    ImGui::SameLine();
                    ImGui::Text("Debug:");
                    if (ImGui::IsItemHovered()) xeditor::hint::Text("Debug level use to compile the resources with");

                    constexpr static std::array items     = { "D0 - None", "D1 - Basic", "Dz - Maximum" };
                    static const float          max_width = ComputeWidthComboBox(items);

                    ImGui::SameLine(0,0);
                    ImGui::PushItemWidth(max_width);

                    int Index = static_cast<int>(m_AssetMgr.m_Compilation.m_Settings.m_DebugLevel);
                    if (ImGui::Combo("##DebugCombo", &Index, items.data(), static_cast<int>(items.size())))
                    {
                        m_AssetMgr.m_Compilation.m_Settings.m_DebugLevel = static_cast<compilation::settings::debug>(Index);
                    }
                    ImGui::PopItemWidth();
                }

                // Optimize
                {
                    ImGui::SameLine();
                    ImGui::Text("Opt:");
                    if (ImGui::IsItemHovered()) xeditor::hint::Text("Optimization level use to compile the resources with");

                    constexpr static std::array items       = { "O0 - None", "O1 - Normal", "Oz - Maximum" };
                    static const float          max_width   = ComputeWidthComboBox(items);

                    ImGui::SameLine(0,0);
                    ImGui::PushItemWidth(max_width);

                    int Index = static_cast<int>(m_AssetMgr.m_Compilation.m_Settings.m_OptimizationLevel);
                    if (ImGui::Combo("##OptimizationCombo", &Index, items.data(), static_cast<int>(items.size())))
                    {
                        m_AssetMgr.m_Compilation.m_Settings.m_OptimizationLevel = static_cast<compilation::settings::optimization>(Index);
                    }
                    ImGui::PopItemWidth();
                }
            }

            //
            // What is shown: only the compiles that had something to say, and a way to forget the finished ones
            //
            {
                ImGui::SameLine();
                if (m_bOnlyProblems) ImGui::PushStyleColor(ImGuiCol_Button, normalColor);
                if (ImGui::Button("Problems only")) m_bOnlyProblems = !m_bOnlyProblems;
                if (m_bOnlyProblems) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) xeditor::hint::Text("Show only the compiles that had warnings or errors");
                ImGui::SameLine();
                if (ImGui::Button("Clear"))
                {
                    std::lock_guard lock(m_AssetMgr.m_Compilation.m_TreesMutex);
                    m_AssetMgr.m_Compilation.m_Trees.clear();
                }
                if (ImGui::IsItemHovered()) xeditor::hint::Text("Forget the compiles that are over");
            }

            //
            // Show the workers working
            //
            if (false)
            {
                ImGui::SameLine();
                int Workers = m_AssetMgr.m_Compilation.m_WorkersWorking.load(std::memory_order_relaxed);
                ImGui::Text("Workers WIP: %d", Workers);
            }
            
            

            //
            // Begin displaying items
            //
            ImGui::BeginChild("##items");
            compile_ui::PruneParseCache();
            if (const auto Asked = m_AssetMgr.m_Compilation.m_FocusInstance.exchange(0)) FocusNodeOf(Asked);
            if (m_NodeUI.size() > 2048) m_NodeUI.clear();

            //
            // The orders that are not over, newest first: a block each, the whole tree of the resource
            //
            // what runs is what a person looks at: those first, then the ones that wait (each newest first)
            auto Open = GroupByRoot(m_AssetMgr.m_Compilation.m_Orders.Snapshot());
            auto Started = [](const order_tree& T)
            {
                for (const auto& N : T.second)
                {
                    if (N.m_bEnded) return true;
                    if (N.m_Log) { xcontainer::lock::scope lock(*N.m_Log); if (N.m_Log->get().m_Result != compilation::historical_entry::result::QUEUED) return true; }
                }
                return false;
            };
            {
                const auto Mid = std::stable_partition(Open.begin(), Open.end(), Started);      // (oldest first inside each group)
                std::reverse(Open.begin(), Mid);
                std::reverse(Mid, Open.end());
            }
            for (auto& T : Open) RenderOrder(T.second, T.first, false, compilation::order_outcome::success);

            //
            // The orders that are over (the last ones), newest first
            //
            bool bAnyOver = false;
            {
                std::lock_guard lock(m_AssetMgr.m_Compilation.m_TreesMutex);
                size_t Shown = 0;
                for (auto it = m_AssetMgr.m_Compilation.m_Trees.rbegin(); it != m_AssetMgr.m_Compilation.m_Trees.rend() && Shown < 50; ++it)
                {
                    if (m_bOnlyProblems && !HasProblems(it->m_Members, it->m_Outcome)) continue;
                    RenderOrder(it->m_Members, it->m_Root, true, it->m_Outcome);
                    bAnyOver = true; ++Shown;
                }
            }

            // nothing to show: say so, calmly
            if (Open.empty() && !bAnyOver)
            {
                const ImVec2 Avail = ImGui::GetContentRegionAvail();
                const char*  pText = m_bOnlyProblems ? "No compile with warnings or errors" : "Nothing has been compiled yet";
                const ImVec2 Size  = ImGui::CalcTextSize(pText);
                ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (Avail.x - Size.x) * 0.5f, ImGui::GetCursorPosY() + std::max(20.0f, Avail.y * 0.35f)));
                ImGui::TextDisabled("%s", pText);
            }

            // sub window
            ImGui::EndChild();
            ImGui::PopStyleColor(); // matches the single ImGuiCol_Button push at the top of this function
        }

    };

    //=============================================================================

    namespace
    {
        // HasLeftPanel=false - LeftPanel() is empty (see the override above), so DOCKABLE mode's own
        // independent Compilation window skips reserving a permanently-blank left column for it.
        inline browser_registration<compiler_tab, "\xEE\x9C\x93 Compilation", 1.0f, false, false > g_CompilationTab{};
    }
}
#endif