namespace xresource_pipeline::descriptor
{
    struct base : xproperty::base
    {
        virtual void SetupFromSource( std::string_view FileName ) = 0;
        virtual void Validate       ( std::vector<std::string>& Errors ) const noexcept = 0;

        virtual xcore::err Serialize( bool isReading, std::string_view Name, xproperty::settings::context& Context ) noexcept
        {
            xcore::textfile::stream Stream;
            if (auto Err = Stream.Open(isReading, Name, xcore::textfile::file_type::TEXT); Err)
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