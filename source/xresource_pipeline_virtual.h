#ifndef XRESOURCE_PIPELINE_VIRTUAL_H
#define XRESOURCE_PIPELINE_VIRTUAL_H
#pragma once

// What a compiler needs to make virtual resources that the user may edit (see documentation/Editors/virtual_resources_plan.md, rule 3).
//
// A virtual resource is an ordinary descriptor in Descriptors/ whose Info.txt links its owner (the resource whose compiler makes it) as the parent. The
// owner keeps the list of the guids it made in its own descriptor folder (Virtual.txt, in source control like the rest): what a slot of a Material Instance
// points at is the generator's while the texture is on that list, the user's otherwise. Each compile:
//   - makes the virtual resources that do not exist
//   - loads the ones that do and updates only the fields the generator owns (the rest is the user's)
//   - writes a file only when its text changed: the time of a file decides a recompile, and a write in Descriptors/ wakes the file watcher
// then writes the list. One that the compile no longer makes stays on disk with its edits (the editor does not compile it).
#include "dependencies/xresource_pipeline_v2/source/xresource_pipeline.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

namespace xresource_pipeline::virtual_rsc
{
    // Writes a file through a temporary one (outside the project: its watcher sees every file) and keeps the old one when nothing changed
    template<typename T_WRITE>
    inline void WriteIfChanged(const std::wstring& Path, T_WRITE&& Write) noexcept
    {
        namespace fs = std::filesystem;
        std::error_code Ec;
        const auto Temp = (fs::temp_directory_path(Ec) / std::format(L"xvirtual_rsc_{:016X}.txt", std::hash<std::wstring>{}(Path))).wstring();
        Write(Temp);

        auto Read = [](const std::wstring& P) { std::ifstream F(fs::path(P), std::ios::binary); return std::string(std::istreambuf_iterator<char>(F), {}); };
        if (!fs::exists(Path, Ec) || Read(Path) != Read(Temp)) fs::copy_file(Temp, Path, fs::copy_options::overwrite_existing, Ec);
        fs::remove(Temp, Ec);
    }

    // The guids an owner made, in "<owner>.desc/Virtual.txt"
    struct owned_list
    {
        std::vector<xresource::full_guid>   m_Resources;

        static std::wstring PathOf(const std::wstring& OwnerFolder) noexcept { return OwnerFolder + L"/Virtual.txt"; }

        bool Contains(const xresource::full_guid& Guid) const noexcept { return std::ranges::find(m_Resources, Guid) != m_Resources.end(); }

        // False when there is no list yet (a project from before the lists: nothing is known to be the generator's)
        bool Load(const std::wstring& OwnerFolder) noexcept
        {
            std::error_code Ec;
            if (!std::filesystem::exists(PathOf(OwnerFolder), Ec)) return false;
            xproperty::settings::context Context;
            return !Serialize(true, PathOf(OwnerFolder), Context);
        }

        void Save(const std::wstring& OwnerFolder) const noexcept
        {
            auto Sorted = *this;                                    // the list is the same text for the same resources, whatever the order they were made in
            std::ranges::sort(Sorted.m_Resources, [](const xresource::full_guid& A, const xresource::full_guid& B) { return A.m_Instance.m_Value < B.m_Instance.m_Value; });
            Sorted.m_Resources.erase(std::ranges::unique(Sorted.m_Resources).begin(), Sorted.m_Resources.end());
            WriteIfChanged(PathOf(OwnerFolder), [&](const std::wstring& P) { xproperty::settings::context C; Sorted.Serialize(false, P, C); });
        }

        xerr Serialize(bool isReading, std::wstring_view FileName, xproperty::settings::context& Context) noexcept
        {
            xtextfile::stream Stream;
            if (auto Err = Stream.Open(isReading, FileName, xtextfile::file_type::TEXT); Err) return Err;
            return xproperty::sprop::serializer::Stream<xproperty::settings::atomic_types_tuple>(Stream, *this, Context);
        }

        XPROPERTY_DEF
        ( "owned_virtual_resources", owned_list
        , obj_member<"Resources", &owned_list::m_Resources>
        )
    };
    XPROPERTY_REG(owned_list)

    // The descriptor of a virtual resource: made from Fresh when it does not exist, otherwise loaded and given only the fields the generator owns
    // (CopyOwned(Existing, Fresh): copy those from Fresh into Existing). Written only when its text changed.
    template<typename T_DESCRIPTOR, typename T_COPY_OWNED>
    inline void EmitDescriptor(const std::wstring& Folder, T_DESCRIPTOR& Fresh, T_COPY_OWNED&& CopyOwned) noexcept
    {
        const auto Path = Folder + L"/Descriptor.txt";
        std::error_code Ec;
        T_DESCRIPTOR Existing;
        xproperty::settings::context Context;
        if (std::filesystem::exists(Path, Ec) && !Existing.Serialize(true, Path, Context))
        {
            CopyOwned(Existing, Fresh);
            WriteIfChanged(Path, [&](const std::wstring& P) { xproperty::settings::context C; Existing.Serialize(false, P, C); });
        }
        else
        {
            WriteIfChanged(Path, [&](const std::wstring& P) { xproperty::settings::context C; Fresh.Serialize(false, P, C); });
        }
    }

    // The Info.txt of a virtual resource: its guid and the link to its owner are the generator's; the name and the rest are the user's once it exists
    inline void EmitInfo(const std::wstring& Folder, const xresource::full_guid Guid, const std::string& Name, const xresource::full_guid Owner) noexcept
    {
        const auto Path = Folder + L"/Info.txt";
        std::error_code Ec;
        xresource_pipeline::info Info{ Guid };
        {
            xproperty::settings::context Context;
            if (std::filesystem::exists(Path, Ec)) (void)Info.Serialize(true, Path, Context);
        }
        if (Info.m_Name.empty()) Info.m_Name = Name;
        Info.m_Guid = Guid;
        if (std::ranges::find(Info.m_RscLinks, Owner) == Info.m_RscLinks.end()) Info.m_RscLinks.push_back(Owner);       // shows as the owner's child in the asset browser
        WriteIfChanged(Path, [&](const std::wstring& P) { xproperty::settings::context C; Info.Serialize(false, P, C); });
    }
}

#endif
