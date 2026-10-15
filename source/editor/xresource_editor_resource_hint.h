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
        std::string Name, TypeName = "<Unknown>", InstanceGuidText, TypeGuidText, InfoReadText, InfoWriteText, DescWriteText, ResWriteText, CommentText;
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

        // What it depends on, as lines under a "Dependencies:" title that use the whole width of the card (a list in the value column of the
        // table wrapped): Level 0 a group ("Resources: 5"), level 1 one of its entries. The resources and the virtual ones are named after the
        // read (naming one reads its node: not while this one is read).
        struct dep_line { int m_Level; std::string m_Text; };
        std::vector<dep_line>               DepLines;
        std::vector<xresource::full_guid>   Resources, VirtualResources;
        bool                                bHasDependencies = false;
        const auto Named = [&](const char* pTitle, const std::vector<xresource::full_guid>& List)
        {
            if (List.empty()) return;
            DepLines.push_back({ 0, std::format("{}: {}", pTitle, List.size()) });
            for (auto& G : List)
            {
                std::string RName = "<Unknown>", RType = "<Unknown>";
                if (!Library.empty()) Mgr.getNodeInfo(Library, G, [&](library_db::info_node& N) { RName = N.m_Info.m_Name; });
                else                  Mgr.getNodeInfo(G, [&](library_db::info_node& N) { RName = N.m_Info.m_Name; });
                if (const auto e = Mgr.m_AssetPluginsDB.m_mPluginsByTypeGUID.find(G.m_Type); e != Mgr.m_AssetPluginsDB.m_mPluginsByTypeGUID.end()) RType = Mgr.m_AssetPluginsDB.m_lPlugins[e->second].m_TypeName;
                DepLines.push_back({ 1, std::format("[{}] {} ({}, {:016X})", static_cast<int>(&G - List.data()), RName, RType, G.m_Instance.m_Value) });
            }
        };
        std::vector<dep_line> AssetLines;
        const auto Paths = [&](const char* pTitle, const std::vector<std::wstring>& List)
        {
            if (List.empty()) return;
            AssetLines.push_back({ 0, std::format("{}: {}", pTitle, List.size()) });
            for (auto& A : List) AssetLines.push_back({ 1, std::format("[{}] {}", static_cast<int>(&A - List.data()), xstrtool::To(A)) });
        };

        const auto Read = [&](library_db::info_node& NodeInfo)
        {
            bHasDependencies = NodeInfo.m_bHasDependencies && NodeInfo.m_Dependencies.hasDependencies();
            if (bHasDependencies)
            {
                Resources        = NodeInfo.m_Dependencies.m_Resources;
                VirtualResources = NodeInfo.m_Dependencies.m_VirtualResources;
                Paths("Assets",         NodeInfo.m_Dependencies.m_Assets);
                Paths("Virtual Assets", NodeInfo.m_Dependencies.m_VirtualAssets);
            }
            Name            = NodeInfo.m_Info.m_Name;
            InfoReadText    = std::format("{:%Y-%m-%d %I:%M:%S %p %Z}", ConvertToStdTime(NodeInfo.m_InfoReadTime));
            InfoWriteText   = std::format("{:%Y-%m-%d %I:%M:%S %p %Z}", ConvertToStdTime(NodeInfo.m_InfoTime));
            DescWriteText   = NodeInfo.m_bHasDescriptor ? std::format("{:%Y-%m-%d %I:%M:%S %p %Z}", ConvertToStdTime(NodeInfo.m_DescriptorTime)) : "Never";
            ResWriteText    = NodeInfo.m_bHasResource   ? std::format("{:%Y-%m-%d %I:%M:%S %p %Z}", ConvertToStdTime(NodeInfo.m_ResourceTime)) : "Never";
            CommentText     = NodeInfo.m_Info.m_Comment;
        };
        if (!Library.empty()) Mgr.getNodeInfo(Library, Guid, [&](library_db::info_node& NodeInfo) { Read(NodeInfo); });
        else                  Mgr.getNodeInfo(Guid, [&](library_db::info_node& NodeInfo) { Read(NodeInfo); });
        if (Name.empty()) Name = "<Unknown>";
        Named("Resources", Resources);
        Named("Virtual Resources", VirtualResources);
        DepLines.insert(DepLines.end(), AssetLines.begin(), AssetLines.end());

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

        const float Left = ImGui::GetCursorPosX();
        ImGui::PushTextWrapPos(Left + 440.0f);
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
            if (!bHasDependencies) Row("Dependencies:", "None");
            ImGui::EndTable();
        }

        // The dependencies from the card's left edge: the whole width for the names and paths
        if (bHasDependencies)
        {
            ImGui::TextDisabled("Dependencies:");
            constexpr float IndentPerLevel = 12.0f;
            for (const auto& L : DepLines)
            {
                // Indent (not a cursor move): a line that wraps goes on under its own start
                const float Indent = IndentPerLevel * float(L.m_Level + 1);
                ImGui::Indent(Indent);
                if (L.m_Level == 0) ImGui::TextDisabled("%s", L.m_Text.c_str());
                else                ImGui::TextUnformatted(L.m_Text.c_str());
                ImGui::Unindent(Indent);
            }
        }

        if (!CommentText.empty())
        {
            ImGui::TextDisabled("Comment:");
            ImGui::Indent(12.0f);
            ImGui::TextUnformatted(CommentText.c_str());
            ImGui::Unindent(12.0f);
        }
        ImGui::PopTextWrapPos();
        g_ResourceHintCard.End();
    }
}

#endif // XRESOURCE_EDITOR_RESOURCE_HINT_H
