#ifndef XRESOURCE_EDITOR_RESOURCE_HINT_H
#define XRESOURCE_EDITOR_RESOURCE_HINT_H
#pragma once

// THE hover card of a resource: what shows when the mouse rests on the picture or the tile of a resource - its name and type, the two ids (instance and type, 16 hex digits like everywhere
// else), when its info, descriptor and compiled resource were last written, what it depends on, its comment, and a big look at its thumbnail when it has one. One place, so the resource
// browser and every inspector that shows a resource draw the same card, and it behaves the same: it starts small and grows to its content, always inside the window it is drawn in
// (xeditor::hint::growing_card), never a window of its own.
//
//      ImGui::Image(...);                                                       // the picture (or the tile) of a resource
//      if (ImGui::IsItemHovered()) xresource_editor::ShowResourceHint(Guid);    // Guid: the xresource::full_guid of the resource; a thumbnail may be passed as the second argument
#include "dependencies/xeditor/include/xeditor/hint.h"
#include "xresource_editor_asset_mgr.h"
#include "imgui.h"

#include <cstdint>
#include <format>
#include <string>

namespace xresource_editor
{
    // The one card every resource hover uses (only one hint is ever open at a time; a new hover starts it small again).
    inline xeditor::hint::growing_card g_ResourceHintCard;

    // Call every frame the mouse rests on the item of a resource, right after the item (the card is placed and measured with it). Thumbnail: the resource's own picture when it has one (the
    // type's glyph is not a thumbnail: pass nothing). Mgr and Library: where to look for the resource; Library empty: in every open library.
    inline void ShowResourceHint(const xresource::full_guid& Guid, const plugin_icon_ref& Thumbnail = {}, library_mgr& Mgr = g_LibMgr, library::guid Library = {}) noexcept
    {
        if (Guid.empty() && Guid.m_Type.empty()) return;                 // not even a type: nothing to say

        // The two ids are the resource's own, so they are there even when no library has it.
        std::string Name, TypeName = "<Unknown>", InstanceGuidText, TypeGuidText, InfoReadText, InfoWriteText, DescWriteText, ResWriteText, DependenciesText, CommentText;
        InstanceGuidText = std::format("{:016X}", Guid.m_Instance.m_Value);
        TypeGuidText     = std::format("{:016X}", Guid.m_Type.m_Value);
        if (const auto e = Mgr.m_AssetPluginsDB.m_mPluginsByTypeGUID.find(Guid.m_Type); e != Mgr.m_AssetPluginsDB.m_mPluginsByTypeGUID.end()) TypeName = Mgr.m_AssetPluginsDB.m_lPlugins[e->second].m_TypeName;

        const auto Row = [](const char* pLabel, const std::string& Value) noexcept
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", pLabel);
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(Value.c_str());
        };

        // A reference that says what type it takes but has nothing in it yet ("(none)"): the card says so, with the type it takes (the same card, so the picture of an empty reference behaves like the others).
        if (Guid.empty())
        {
            const std::uint64_t TypeKey = Guid.m_Type.m_Value * 0x9E3779B97F4A7C15ull;
            g_ResourceHintCard.Begin(static_cast<ImGuiID>((TypeKey ^ (TypeKey >> 32)) | 1u), 480.0f);
            if (ImGui::BeginTable("##ResourceHintNone", 2, ImGuiTableFlags_SizingFixedFit))
            {
                Row("Instance Name:", "(none)");
                Row("Type Name:",     TypeName);
                Row("Instance GUID:", "(none)");
                Row("Type GUID:",     TypeGuidText);
                ImGui::EndTable();
            }
            ImGui::TextDisabled("Nothing is referenced yet: press the name to pick a resource.");
            g_ResourceHintCard.End();
            return;
        }

        const auto Read = [&](library_db::info_node& NodeInfo)
        {
            Name            = NodeInfo.m_Info.m_Name;
            InfoReadText    = std::format("{:%Y-%m-%d %I:%M:%S %p %Z}", ConvertToStdTime(NodeInfo.m_InfoReadTime));
            InfoWriteText   = std::format("{:%Y-%m-%d %I:%M:%S %p %Z}", ConvertToStdTime(NodeInfo.m_InfoTime));
            DescWriteText   = NodeInfo.m_bHasDescriptor ? std::format("{:%Y-%m-%d %I:%M:%S %p %Z}", ConvertToStdTime(NodeInfo.m_DescriptorTime)) : "Never";
            ResWriteText    = NodeInfo.m_bHasResource   ? std::format("{:%Y-%m-%d %I:%M:%S %p %Z}", ConvertToStdTime(NodeInfo.m_ResourceTime)) : "Never";

            DependenciesText = [&]() -> std::string
            {
                if (NodeInfo.m_bHasDependencies == false || false == NodeInfo.m_Dependencies.hasDependencies()) return { "No Dependencies" };

                std::string Dependencies;
                if (NodeInfo.m_Dependencies.m_Resources.empty() == false)
                    Dependencies += std::format("\n    Resources Count: {}", NodeInfo.m_Dependencies.m_Resources.size());

                if (NodeInfo.m_Dependencies.m_Assets.empty() == false)
                {
                    std::string Assets = std::format("\n    Asset Count: {}", NodeInfo.m_Dependencies.m_Assets.size());
                    for (auto& A : NodeInfo.m_Dependencies.m_Assets)
                        Assets = std::format("{}\n        [{}] {}", Assets, static_cast<int>(&A - NodeInfo.m_Dependencies.m_Assets.data()), xstrtool::To(A));
                    Dependencies += Assets;
                }

                if (NodeInfo.m_Dependencies.m_VirtualAssets.empty() == false)
                {
                    std::string Assets = std::format("\n    Virtual Asset Count: {}", NodeInfo.m_Dependencies.m_VirtualAssets.size());
                    for (auto& A : NodeInfo.m_Dependencies.m_VirtualAssets)
                        Assets = std::format("{}\n        [{}] {}", Assets, static_cast<int>(&A - NodeInfo.m_Dependencies.m_VirtualAssets.data()), xstrtool::To(A));
                    Dependencies += Assets;
                }
                return Dependencies;
            }();

            CommentText = NodeInfo.m_Info.m_Comment;
        };
        if (!Library.empty()) Mgr.getNodeInfo(Library, Guid, [&](library_db::info_node& NodeInfo) { Read(NodeInfo); });
        else                  Mgr.getNodeInfo(Guid, [&](library_db::info_node& NodeInfo) { Read(NodeInfo); });
        if (Name.empty()) Name = "<Unknown>";

        // Starts small and grows to its content, always inside the window it is drawn in.
        // Owned by the resource (not by the item: an image has no id of its own), so the size measured for one resource is known the next time its card opens, wherever it is hovered.
        const std::uint64_t Key = Guid.m_Instance.m_Value ^ (Guid.m_Type.m_Value * 0x9E3779B97F4A7C15ull);
        g_ResourceHintCard.Begin(static_cast<ImGuiID>((Key ^ (Key >> 32)) | 1u), 480.0f);

        // A real thumbnail gets a big preview at the top with the four identity rows beside it (the columns of a table align by pixel width, so it is right in a proportional font);
        // without one the same rows lead the full-width table.
        const bool bHasThumbnail = Thumbnail.isValid();
        if (bHasThumbnail)
        {
            constexpr float PreviewSize = 128.0f;
            ImGui::Image((ImTextureRef)(void*)Thumbnail.m_pTexture, ImVec2(PreviewSize, PreviewSize), ImVec2(Thumbnail.m_U0, Thumbnail.m_V0), ImVec2(Thumbnail.m_U1, Thumbnail.m_V1));
            ImGui::SameLine();
            ImGui::BeginGroup();
            if (ImGui::BeginTable("##ResourceHintHeader", 2, ImGuiTableFlags_SizingFixedFit))
            {
                Row("Instance Name:", Name);
                Row("Type Name:",     TypeName);
                Row("Instance GUID:", InstanceGuidText);
                Row("Type GUID:",     TypeGuidText);
                ImGui::EndTable();
            }
            ImGui::EndGroup();
            ImGui::Spacing();
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 440.0f);
        if (ImGui::BeginTable("##ResourceHint", 2, ImGuiTableFlags_SizingFixedFit))
        {
            if (!bHasThumbnail)
            {
                Row("Instance Name:", Name);
                Row("Type Name:",     TypeName);
                Row("Instance GUID:", InstanceGuidText);
                Row("Type GUID:",     TypeGuidText);
            }
            Row("Info Last Read:",         InfoReadText);
            Row("Info Last Write:",        InfoWriteText);
            Row("Descriptor Last Write:",  DescWriteText);
            Row("Resource Last Write:",    ResWriteText);
            Row("Dependencies:",           DependenciesText);
            Row("Comment:",                CommentText);
            ImGui::EndTable();
        }
        ImGui::PopTextWrapPos();
        g_ResourceHintCard.End();
    }
}

#endif // XRESOURCE_EDITOR_RESOURCE_HINT_H
