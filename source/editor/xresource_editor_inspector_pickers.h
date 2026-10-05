#ifndef XRESOURCE_EDITOR_INSPECTOR_PICKERS_H
#define XRESOURCE_EDITOR_INSPECTOR_PICKERS_H
#pragma once

// Resource picking for xproperty inspectors: a property that references a resource shows the resource's name as a button
// and opens the asset browser as a popup to choose another. Used by every editor with resource-reference properties.
#include "dependencies/xresource_pipeline_v2/source/editor/xresource_editor_asset_browser.h"
#include "dependencies/xproperty/source/examples/imgui/xPropertyImGuiInspector.h"

#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <span>
#include <string>

namespace xresource_editor
{
    //---------------------------------------------------------------------------
    // Resource-picker wiring - same trio every editor with a resource-ref property carries its own
    // copy of (see E24_AnimPackage_Editor.cpp's identical RemapGUIDToString/RenderResourceWigzmos/
    // ResourceBrowserPopup). Used both by the shared inspectors' delegates and directly by the
    // Scene panel's own "Parent Scenes" row list.
    //---------------------------------------------------------------------------

    inline void RemapGUIDToString(std::string& Out, const xresource::full_guid& PreFullGuid)
    {
        if (PreFullGuid.empty())
        {
            Out = "(none)";
            return;
        }

        auto FullGuid = xresource::g_Mgr.getFullGuid(PreFullGuid);
        Out.clear();
        xresource_editor::g_LibMgr.getNodeInfo(FullGuid, [&](xresource_editor::library_db::info_node& Node) { Out = Node.m_Info.m_Name; });
        if (Out.empty()) Out = std::format("{:X}", FullGuid.m_Instance.m_Value);
    }

    //---------------------------------------------------------------------------
    // The resource reference of an inspector: ONE widget for every property that references a resource. What it needs from the application (the picture of a resource, opening its editor, finding it
    // in the resource browser) comes through these hooks, which the application fills once; without them the widget is the name and the picker only.
    //---------------------------------------------------------------------------
    struct reference_host
    {
        std::function<plugin_icon_ref(xresource::full_guid)>    m_Thumbnail;        // the picture of the resource itself, when it has one that is ready (invalid: not yet)
        std::function<bool(xresource::type_guid)>               m_HasEditor;        // the type of resource has an editor
        std::function<void(xresource::full_guid)>               m_OpenEditor;       // open (or bring forward) the editor of the resource
        std::function<bool(xresource::full_guid)>               m_Locate;           // find the resource in the resource browser (of the drawer): false when it cannot be shown there
    };
    inline reference_host g_ReferenceHost;

    // Draws the picture of a resource: its own thumbnail when it has one, the picture of its type otherwise.
    inline void RenderReferencePicture(const xresource::full_guid& Guid, float Size) noexcept
    {
        plugin_icon_ref Picture;
        if (g_ReferenceHost.m_Thumbnail && !Guid.empty()) Picture = g_ReferenceHost.m_Thumbnail(Guid);
        if (!Picture.isValid()) Picture = xresource_editor::g_LibMgr.m_AssetPluginsDB.getIconRef(Guid.m_Type, 0);
        if (Picture.isValid())
            ImGui::Image((ImTextureRef)(void*)Picture.m_pTexture, ImVec2(Size, Size), ImVec2(Picture.m_U0, Picture.m_V0), ImVec2(Picture.m_U1, Picture.m_V1));
        else
            ImGui::Dummy(ImVec2(Size, Size));
    }

    // A resource reference: the picture, the name (a press opens the picker), and the actions of the reference: open the resource in its editor, find it in the resource browser, clear the
    // reference. Big: the picture beside two lines, the name with the clear button at its right, and the two buttons under it. Small (the property's SMALL_RESOURCE flag, for lists where a row
    // has no room): one line, the actions in the menu of a button at the right of the name.
    inline void RenderResourceReference(xproperty::inspector& Inspector, bool& bOpen, const xresource::full_guid& PreFullGuid) noexcept
    {
        constexpr const char* OpenIcon = "\xEE\x9C\x8F", * LocateIcon = "\xEE\xA0\xB8", * ClearIcon = "\xEE\x9C\x91", * MenuIcon = "\xEE\x9C\x92";    // Segoe MDL2: Edit, FolderOpen, Cancel, More

        const bool bSmall = Inspector.m_CurrentProperty.m_Flags.m_bSmallResource;
        const bool bNone  = PreFullGuid.empty();
        std::string Name;
        RemapGUIDToString(Name, PreFullGuid);
        const auto  Full  = bNone ? PreFullGuid : xresource::g_Mgr.getFullGuid(PreFullGuid);
        bool bKnown = false;
        if (!bNone) xresource_editor::g_LibMgr.getNodeInfo(Full, [&](xresource_editor::library_db::info_node&) { bKnown = true; });

        const bool bCanOpen   = bKnown && g_ReferenceHost.m_OpenEditor && g_ReferenceHost.m_HasEditor && g_ReferenceHost.m_HasEditor(Full.m_Type);
        const bool bCanLocate = bKnown && g_ReferenceHost.m_Locate;
        const auto Open       = [&] { g_ReferenceHost.m_OpenEditor(Full); };
        const auto Locate     = [&] { g_ReferenceHost.m_Locate(Full); };

        const auto& Style = ImGui::GetStyle();
        const float Line  = ImGui::GetFrameHeight();
        ImGui::PushID(reinterpret_cast<const void*>(std::hash<std::string_view>{}(Inspector.m_CurrentProperty.m_Path)));

        // The name button: red when the reference names a resource that no open library has.
        const auto NameButton = [&](float Width, float Height)
        {
            const bool bBroken = !bNone && !bKnown;
            if (bBroken) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.45f, 0.42f, 1.0f));
            bOpen = ImGui::Button((Name + "###name").c_str(), ImVec2(Width, Height));
            if (bBroken) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered() && bBroken) ImGui::SetTooltip("No open library has this resource");
        };
        const auto Tip = [](const char* pText) { if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", pText); };

        if (bSmall)
        {
            RenderReferencePicture(Full, Line);
            ImGui::SameLine();
            NameButton(-(Line + Style.ItemSpacing.x), 0.0f);
            ImGui::SameLine();
            if (ImGui::Button(MenuIcon, ImVec2(Line, 0.0f))) ImGui::OpenPopup("##referencemenu");
            Tip("Open in its editor, find in the resource browser, clear");
            if (ImGui::BeginPopup("##referencemenu"))
            {
                if (ImGui::MenuItem("Open in its editor", nullptr, false, bCanOpen)) Open();
                if (ImGui::MenuItem("Find in the resource browser", nullptr, false, bCanLocate)) Locate();
                ImGui::Separator();
                if (ImGui::MenuItem("Clear", nullptr, false, !bNone)) Inspector.m_CurrentProperty.m_bClearResource = true;
                ImGui::EndPopup();
            }
        }
        else
        {
            const float Picture = xproperty::inspector::ResourceRowHeight(false);          // as tall as the label at its left
            ImGui::BeginGroup();
            RenderReferencePicture(Full, Picture);
            ImGui::SameLine();
            ImGui::BeginGroup();
            NameButton(-(Line + Style.ItemSpacing.x), Line);
            ImGui::SameLine();
            ImGui::BeginDisabled(bNone);
            if (ImGui::Button(ClearIcon, ImVec2(Line, Line))) Inspector.m_CurrentProperty.m_bClearResource = true;
            ImGui::EndDisabled();
            Tip("Clear the reference");

            ImGui::BeginDisabled(!bCanOpen);
            if (ImGui::Button(OpenIcon, ImVec2(Line * 1.5f, Line))) Open();
            ImGui::EndDisabled();
            Tip(bCanOpen ? "Open the resource in its editor" : "This resource has no editor");
            ImGui::SameLine();
            ImGui::BeginDisabled(!bCanLocate);
            if (ImGui::Button(LocateIcon, ImVec2(Line * 1.5f, Line))) Locate();
            ImGui::EndDisabled();
            Tip("Find the resource in the resource browser");
            ImGui::EndGroup();
            ImGui::EndGroup();
        }
        ImGui::PopID();
    }

    //---------------------------------------------------------------------------
    // The asset reference of an inspector: ONE widget for every property that names a source file of the project (xproperty::ui::g_AssetFileWidget, which InstallAssetFileWidget sets), the
    // twin of the resource reference above. What it needs from the application (open the file the way the Assets tab does, find it in the Assets tab) comes through these hooks, which the
    // application fills once; without them the actions are off.
    //---------------------------------------------------------------------------
    struct asset_reference_host
    {
        std::function<bool(const std::wstring&)>    m_Open;         // open the file as a double click on it in the Assets tab does; false when no library has it
        std::function<bool(const std::wstring&)>    m_Locate;       // find the file in the Assets tab (of the drawer); false when it cannot be shown there
    };
    inline asset_reference_host g_AssetReferenceHost;

    // The file a descriptor names ("Assets\Folder\file.png", relative to its library, or a full path) on disk; empty when no open library has it.
    inline std::filesystem::path ResolveAssetFile(const std::wstring& Path) noexcept
    {
        if (Path.empty()) return {};
        std::error_code Ec;
        const std::filesystem::path Given(Path);
        if (Given.is_absolute()) return std::filesystem::exists(Given, Ec) ? Given : std::filesystem::path{};
        for (auto& L : xresource_editor::g_LibMgr.m_mLibraryDB)
        {
            auto Candidate = std::filesystem::path(L.second->m_Library.m_Path) / Given;
            if (std::filesystem::exists(Candidate, Ec)) return Candidate;
        }
        return {};
    }

    // Whether a file name is one of the types of a file dialog filter ("Name\0*.png;*.jpg\0Name2\0*.tga\0\0"): the property takes a file of those types. No filter: every file.
    inline bool AssetFilterAccepts(const wchar_t* pFilter, const std::filesystem::path& File) noexcept
    {
        if (pFilter == nullptr) return true;
        auto Lower = [](std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c)); return s; };
        const std::wstring Name = Lower(File.filename().wstring());
        for (const wchar_t* p = pFilter; *p; )
        {
            p += std::wcslen(p) + 1;                                    // the name of the group
            if (!*p) break;
            std::wstring_view Patterns(p);
            p += Patterns.size() + 1;
            while (!Patterns.empty())
            {
                const auto End     = Patterns.find(L';');
                auto Pattern = Lower(std::wstring(Patterns.substr(0, End)));
                Pattern.erase(0, Pattern.find_first_not_of(L" \t"));                    // the filters of the descriptors have a space after each ";" (" *.png; *.tga")
                Pattern.erase(Pattern.find_last_not_of(L" \t") + 1);
                if (Pattern == L"*" || Pattern == L"*.*") return true;
                if (Pattern.size() > 1 && Pattern[0] == L'*' && Name.size() >= Pattern.size() - 1 && Name.compare(Name.size() - (Pattern.size() - 1), std::wstring::npos, Pattern, 1, std::wstring::npos) == 0) return true;
                if (End == std::wstring_view::npos) break;
                Patterns.remove_prefix(End + 1);
            }
        }
        return false;
    }

    // The file dropped on the widget, from the Assets tab ("XRESOURCE_EDITOR_ASSET_FILE_DRAG"): the path the property keeps for it. False when what is dragged is not a file of the types the property takes.
    inline bool AcceptDroppedAssetFile(const xproperty::ui::asset_file_request& Request, std::wstring& Out) noexcept
    {
        if (!ImGui::BeginDragDropTarget()) return false;
        bool bTaken = false;
        if (const ImGuiPayload* pPayload = ImGui::GetDragDropPayload(); pPayload && pPayload->IsDataType("XRESOURCE_EDITOR_ASSET_FILE_DRAG") && pPayload->DataSize == sizeof(asset_file_drag_payload))
        {
            const auto& Drag = *static_cast<const asset_file_drag_payload*>(pPayload->Data);
            const std::filesystem::path Rel(Drag.m_SourcePath);
            std::error_code Ec;
            for (auto& L : xresource_editor::g_LibMgr.m_mLibraryDB)
            {
                if (L.first != Drag.m_Library) continue;
                const std::filesystem::path Root = L.second->m_Library.m_Path;
                const auto Full = Root / L"Assets" / Rel;
                if (std::filesystem::is_directory(Full, Ec) || !std::filesystem::exists(Full, Ec) || !AssetFilterAccepts(Request.m_pFilter, Full)) break;
                if (ImGui::AcceptDragDropPayload("XRESOURCE_EDITOR_ASSET_FILE_DRAG"))
                {
                    Out    = Request.m_bMakePathRelative ? (std::filesystem::path(L"Assets") / Rel).wstring() : Full.wstring();
                    bTaken = true;
                }
                break;
            }
        }
        ImGui::EndDragDropTarget();
        return bTaken;
    }

    // An asset reference: the name of the file on a line (a press opens the file dialog to choose another; the clear button at its right), and under it the actions: open the file the way the
    // Assets tab does, find it in the Assets tab. Dropping a file of the types the property takes from the Assets tab on it sets the property. As tall as the resource reference: two lines.
    inline bool RenderAssetReference(xproperty::inspector& Inspector, const xproperty::ui::asset_file_request& Request, std::wstring& NewValue) noexcept
    {
        constexpr const char* OpenIcon = "\xEE\x9C\x8F", * LocateIcon = "\xEE\xA0\xB8", * ClearIcon = "\xEE\x9C\x91";    // Segoe MDL2: Edit, FolderOpen, Cancel

        const bool bNone   = Request.m_Value.empty();
        const auto Resolved = ResolveAssetFile(Request.m_Value);
        const bool bKnown  = !Resolved.empty();
        const bool bCanOpen   = bKnown && g_AssetReferenceHost.m_Open;
        const bool bCanLocate = bKnown && g_AssetReferenceHost.m_Locate;

        const auto& Style = ImGui::GetStyle();
        const float Line  = ImGui::GetFrameHeight();
        const auto  Tip   = [](const char* pText) { if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", pText); };
        bool bChanged = false;

        std::string Name = "(none)";
        if (!bNone) Name = std::filesystem::path(Request.m_Value).filename().string();

        ImGui::PushID(reinterpret_cast<const void*>(std::hash<std::string_view>{}(Inspector.m_CurrentProperty.m_Path)));
        ImGui::BeginGroup();

        // The name: red when the property names a file that no open library has.
        const bool bBroken = !bNone && !bKnown;
        if (bBroken) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.45f, 0.42f, 1.0f));
        const bool bPick = ImGui::Button((Name + "###name").c_str(), ImVec2(-(Line + Style.ItemSpacing.x), Line));
        if (bBroken) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            const std::string Full = bNone ? std::string("No file. Press to choose one, or drop one from the Assets tab") : std::filesystem::path(Request.m_Value).string();
            ImGui::SetTooltip("%s%s", Full.c_str(), bBroken ? "\nNo open library has this file" : "");
        }
        if (bPick)
        {
            std::wstring Chosen;
            if (Request.m_Browse && Request.m_Browse(Chosen)) { NewValue = Chosen; bChanged = true; }
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(bNone);
        if (ImGui::Button(ClearIcon, ImVec2(Line, Line))) { NewValue.clear(); bChanged = true; }
        ImGui::EndDisabled();
        Tip("Clear the file");

        ImGui::BeginDisabled(!bCanOpen);
        if (ImGui::Button(OpenIcon, ImVec2(Line * 1.5f, Line))) g_AssetReferenceHost.m_Open(Request.m_Value);
        ImGui::EndDisabled();
        Tip(bCanOpen ? "Open the file, as the Assets tab does" : "There is no such file in the open libraries");
        ImGui::SameLine();
        ImGui::BeginDisabled(!bCanLocate);
        if (ImGui::Button(LocateIcon, ImVec2(Line * 1.5f, Line))) g_AssetReferenceHost.m_Locate(Request.m_Value);
        ImGui::EndDisabled();
        Tip("Find the file in the Assets tab");

        ImGui::EndGroup();
        std::wstring Dropped;
        if (AcceptDroppedAssetFile(Request, Dropped)) { NewValue = Dropped; bChanged = true; }
        ImGui::PopID();
        return bChanged;
    }

    // Makes this widget the one of every property that names a file of the assets (xproperty::ui::g_AssetFileWidget). The application calls it once, where it fills g_AssetReferenceHost.
    inline void InstallAssetFileWidget() noexcept
    {
        xproperty::ui::g_AssetFileWidget.m_Draw = &RenderAssetReference;
    }

    inline xresource_editor::asset_browser g_AssetBrowserPopup;

    // NOTE: only safe to call with an `Open` that is a genuinely FRESH per-frame local (e.g. declared
    // inside a loop body, or an inspector row's own transient state) - never a persistent member
    // variable. g_AssetBrowserPopup.RenderAsPopup() (called once, early, each frame) already closes
    // the popup and clears its owner id when the user hits its own Close button; if `Open` is a
    // persistent flag that nothing else resets, the very next line below (`if (Open && not
    // isVisible())`) misreads that as a fresh open request and reopens it immediately - an instant,
    // permanent close/reopen loop with no way for the user to actually close it. Call sites that need
    // to track "please open" across frames (e.g. a tree row's own "+" button) should call
    // ShowAsPopup(...) directly on the click itself instead of routing through this function.
    inline void ResourceBrowserPopup(const void* pUID, bool& Open, xresource::full_guid& Output, std::span<const xresource::type_guid> Filters)
    {
        // Drag-and-drop onto this property's own wigzmo button, from a resource tile dragged out of the
        // asset browser (xresource_editor_asset_browser_virtual_tree_tab.h's own "DESCRIPTOR_GUID" payload). This was
        // simply never implemented here - confirmed live: every WireResourcePickerCallbacks consumer
        // (every editor using the generic picker, not just this one) could only ever assign a reference
        // by clicking the button to open the browse popup; dragging silently did nothing, not because of
        // any ID/state bug, but because this function never checked for a drag payload at all. Mirrors
        // xgpu_editor_resource_picker.h's own ResourceBrowserPopup (E21's separate, drag-drop-capable
        // picker) - same payload struct/type, so a tile dragged from the browser works against either.
        if (ImGui::BeginDragDropTarget())
        {
            struct drag_and_drop_folder_payload_t
            {
                xresource_editor::folder::guid    m_Parent;
                xresource::full_guid m_Source;
                bool                 m_bSelection;
            };

            if (const ImGuiPayload* payload = ImGui::GetDragDropPayload(); payload && payload->IsDataType("DESCRIPTOR_GUID"))
            {
                IM_ASSERT(payload->DataSize == sizeof(drag_and_drop_folder_payload_t));
                auto& PayloadData = *static_cast<const drag_and_drop_folder_payload_t*>(payload->Data);

                bool bAccept = Output.m_Type == PayloadData.m_Source.m_Type;
                if (not bAccept) for (auto& Type : Filters) if (PayloadData.m_Source.m_Type == Type) { bAccept = true; break; }

                if (bAccept && ImGui::AcceptDragDropPayload("DESCRIPTOR_GUID"))
                {
                    Output = PayloadData.m_Source;
                    if (g_AssetBrowserPopup.isVisible()) g_AssetBrowserPopup.ClosePopup();
                    Open = false;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (g_AssetBrowserPopup.getCurrentID() != nullptr && g_AssetBrowserPopup.getCurrentID() != pUID)
            return;

        if (Open && not g_AssetBrowserPopup.isVisible())
            g_AssetBrowserPopup.ShowAsPopup(xresource_editor::g_LibMgr, pUID, Filters, Output.m_Type);

        if (auto SelectedAsset = g_AssetBrowserPopup.getSelectedAsset(); SelectedAsset.empty() == false)
        {
            // The property's own type is always accepted (Output.m_Type): a plain resource reference has no filter list of its own
            bool bAccept = SelectedAsset.m_Type == Output.m_Type;
            for (auto& Type : Filters)
                if (SelectedAsset.m_Type == Type) { bAccept = true; break; }
            if (bAccept) Output = SelectedAsset;
        }

        Open = g_AssetBrowserPopup.isVisible();
    }

    // Registers the two stateless resource-picker delegates (m_OnResourceWigzmos/m_OnResourceBrowser)
    // on an entity/component inspector - identical wiring every editor with a resource-ref property
    // needs, extracted here so it's one call instead of re-typing both Register<...> lambdas per
    // editor.
    inline void WireResourcePickerCallbacks(xproperty::inspector& Inspector) noexcept
    {
        Inspector.m_OnResourceWigzmos.m_Delegates.clear();      // an editor that wired another widget before gets this one: the reference is ONE widget for the whole system
        Inspector.m_OnResourceWigzmos.Register<[](xproperty::inspector& Insp, const xproperty::type::object&, void*, std::string_view, bool& bOpen, const xresource::full_guid& PreFullGuid)
        {
            xresource_editor::RenderResourceReference(Insp, bOpen, PreFullGuid);
        }>();
        Inspector.m_OnResourceBrowser.Register<[](xproperty::inspector&, const xproperty::type::object&, void*, std::string_view Path, bool& bOpen, xresource::full_guid& Out, std::span<const xresource::type_guid> Filters)
        {
            const void* pUID = reinterpret_cast<const void*>(std::hash<std::string_view>{}(Path));
            xresource_editor::ResourceBrowserPopup(pUID, bOpen, Out, Filters);
        }>();
    }
}

#endif // XRESOURCE_EDITOR_INSPECTOR_PICKERS_H
