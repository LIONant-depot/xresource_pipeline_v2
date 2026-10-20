#pragma once
#include <filesystem>

namespace xresource_pipeline::descriptor
{
    struct base : xproperty::base
    {
        virtual void SetupFromSource( std::string_view FileName ) = 0;
        virtual void Validate       ( std::vector<std::string>& Errors ) const noexcept = 0;
        virtual void SetupNew       ( void ) noexcept {}        // a descriptor the user just made (not read): the defaults a new resource gets that one from before them, read without the field, must not

        // The most descriptive name this descriptor can give its resource (the name of the file it imports); empty when it has none yet. An editor uses it to name a
        // resource whose name is still the default.
        virtual std::string SuggestedName( void ) const noexcept { return {}; }
        static std::string NameOfFile( std::wstring_view Path ) noexcept
        {
            const auto U8 = std::filesystem::path(Path).stem().u8string();
            return std::string(U8.begin(), U8.end());
        }

        virtual xerr Serialize( bool isReading, std::wstring_view FileName, xproperty::settings::context& Context ) noexcept
        {
            xtextfile::stream Stream;
            if (auto Err = Stream.Open(isReading, FileName, xtextfile::file_type::TEXT); Err)
                return Err;

            if (auto Err = m_Version.SerializeVersion(Stream); Err)
                return Err;

            if ( auto Err = xproperty::sprop::serializer::Stream<xproperty::settings::atomic_types_tuple>(Stream, *this, Context); Err)
                return Err;

            return {};
        }

        version     m_Version{};
    };
}