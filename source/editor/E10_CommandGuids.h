#ifndef E10_COMMAND_GUIDS_H
#define E10_COMMAND_GUIDS_H
#pragma once

// How the resource commands (E10_Commands_*.h) write and read library and asset guids on the command line.
#include "dependencies/xresource_pipeline_v2/source/editor/E10_AssetMgr.h"
#include <format>
#include <string>
#include <string_view>

namespace e10::commands
{
    // Same shape as ParseSceneGuid/FormatSceneGuid - e10::library::guid is also a
    // xresource::def_guid<> (a single instance value, implicit compile-time type), same as
    // xecs::scene::guid/xecs::level::guid. Named separately (not just reused via a template) so a
    // command's own signature stays self-documenting about which guid space an argument names.
    inline e10::library::guid ParseLibraryGuid(std::string_view Text) noexcept
    {
        return e10::library::guid{ .m_Instance = { std::strtoull(std::string(Text).c_str(), nullptr, 16) } };
    }

    inline std::string FormatLibraryGuid(e10::library::guid Guid) noexcept
    {
        return std::format("{:016X}", Guid.m_Instance.m_Value);
    }

    // An asset guid (xresource::full_guid) is genuinely instance+type - unlike Scene/Level/Library's
    // def_guid<> (one fixed, compile-time-known type), an asset can be a Prefab, Scene, Level,
    // Texture, Material, ... resource, so the type half has to travel too. 32 hex digits (16 instance
    // + 16 type, concatenated with no separator) keeps this a single command-line token, same
    // "one guid, one argument" shape every other guid convention here already has.
    inline xresource::full_guid ParseAssetGuid(std::string_view Text) noexcept
    {
        if (Text.size() != 32) return {};
        const auto Instance = std::strtoull(std::string(Text.substr(0, 16)).c_str(), nullptr, 16);
        const auto Type     = std::strtoull(std::string(Text.substr(16, 16)).c_str(), nullptr, 16);
        return xresource::full_guid{ .m_Instance = { Instance }, .m_Type = { Type } };
    }

    inline std::string FormatAssetGuid(xresource::full_guid Guid) noexcept
    {
        return std::format("{:016X}{:016X}", Guid.m_Instance.m_Value, Guid.m_Type.m_Value);
    }
}

#endif // E10_COMMAND_GUIDS_H
