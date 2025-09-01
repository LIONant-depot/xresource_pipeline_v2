namespace xresource_pipeline
{
    struct version
    {
        std::int32_t    m_Major = 1;
        std::int32_t    m_Minor = 0;

        inline xerr   SerializeVersion(xtextfile::stream& Stream) noexcept
        {
            return Stream.Record("DescriptorVersion", [&](xerr& Error)
                { 0 || (Error = Stream.Field("Major", m_Major))
                    || (Error = Stream.Field("Minor", m_Minor))
                    ;
                });
        }

        XPROPERTY_DEF
        ("version", version, xproperty::settings::vector2_group
        , obj_member_ro< "Major", &version::m_Major >
        , obj_member_ro< "minor", &version::m_Minor >
        )
    };
    XPROPERTY_REG(version)
}