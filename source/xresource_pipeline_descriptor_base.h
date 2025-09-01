namespace xresource_pipeline::descriptor
{
    struct base : xproperty::base
    {
        virtual void SetupFromSource( std::string_view FileName ) = 0;
        virtual void Validate       ( std::vector<std::string>& Errors ) const noexcept = 0;

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