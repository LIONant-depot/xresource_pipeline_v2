#ifndef E10_INSPECTOR_PICKERS_H
#define E10_INSPECTOR_PICKERS_H
#pragma once

// Resource picking for xproperty inspectors: a property that references a resource shows the resource's name as a button
// and opens the asset browser as a popup to choose another. Used by every editor with resource-reference properties.
#include "dependencies/xresource_pipeline_v2/source/editor/E10_AssetBrowser.h"
#include "dependencies/xproperty/source/examples/imgui/xPropertyImGuiInspector.h"

#include <span>
#include <string>

namespace e10
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
        e10::g_LibMgr.getNodeInfo(FullGuid, [&](e10::library_db::info_node& Node) { Out = Node.m_Info.m_Name; });
        if (Out.empty()) Out = std::format("{:X}", FullGuid.m_Instance.m_Value);
    }

    inline void RenderResourceWigzmos(bool& bOpen, const xresource::full_guid& PreFullGuid)
    {
        std::string Name;
        RemapGUIDToString(Name, PreFullGuid);
        bOpen = ImGui::Button(Name.c_str(), ImVec2(-1, 0));
    }

    inline e10::assert_browser g_AssetBrowserPopup;

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
        if (g_AssetBrowserPopup.getCurrentID() != nullptr && g_AssetBrowserPopup.getCurrentID() != pUID)
            return;

        if (Open && not g_AssetBrowserPopup.isVisible())
            g_AssetBrowserPopup.ShowAsPopup(e10::g_LibMgr, pUID, Filters, Output.m_Type);

        if (auto SelectedAsset = g_AssetBrowserPopup.getSelectedAsset(); SelectedAsset.empty() == false)
        {
            for (auto& Type : Filters)
                if (SelectedAsset.m_Type == Type) { Output = SelectedAsset; break; }
        }

        Open = g_AssetBrowserPopup.isVisible();
    }

    // Registers the two stateless resource-picker delegates (m_OnResourceWigzmos/m_OnResourceBrowser)
    // on an entity/component inspector - identical wiring every editor with a resource-ref property
    // needs, extracted here so it's one call instead of re-typing both Register<...> lambdas per
    // editor.
    inline void WireResourcePickerCallbacks(xproperty::inspector& Inspector) noexcept
    {
        Inspector.m_OnResourceWigzmos.Register<[](xproperty::inspector&, const xproperty::type::object&, void*, std::string_view, bool& bOpen, const xresource::full_guid& PreFullGuid)
        {
            e10::RenderResourceWigzmos(bOpen, PreFullGuid);
        }>();
        Inspector.m_OnResourceBrowser.Register<[](xproperty::inspector&, const xproperty::type::object&, void*, std::string_view Path, bool& bOpen, xresource::full_guid& Out, std::span<const xresource::type_guid> Filters)
        {
            const void* pUID = reinterpret_cast<const void*>(std::hash<std::string_view>{}(Path));
            e10::ResourceBrowserPopup(pUID, bOpen, Out, Filters);
        }>();
    }
}

#endif // E10_INSPECTOR_PICKERS_H
