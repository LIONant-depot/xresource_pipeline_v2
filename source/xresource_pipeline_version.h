namespace xresource_pipeline
{
    struct version
    {
        std::int32_t    m_Major = 1;
        std::int32_t    m_Minor = 0;

        inline xcore::err   SerializeVersion(xcore::textfile::stream& Stream) noexcept
        {
            xcore::err Error;

            Stream.Record(Error, "DescriptorVersion", [&](xcore::err& Error)
                { 0 || Stream.Field("Major", m_Major).isError(Error)
                    || Stream.Field("Minor", m_Minor).isError(Error)
                    ;
                });
            return Error;
        }

        XPROPERTY_DEF
        ("version", version, xproperty::settings::vector2_group
            , obj_member_ro< "Major", &version::m_Major >
            , obj_member_ro< "minor", &version::m_Minor >
        )
    };
    XPROPERTY_REG(version)
}