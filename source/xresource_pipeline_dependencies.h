namespace xresource_pipeline
{
    struct guid_full
    {
        std::uint64_t m_Value;
        std::uint64_t m_Type;

        XPROPERTY_DEF
        ( "guid_full", guid_full
        , obj_member<"m_Value", &guid_full::m_Value>
        , obj_member<"m_Type",  &guid_full::m_Type >
        )
    };
    XPROPERTY_REG(guid_full)

    struct dependencies
    {
        std::vector<guid_full>      m_Resources;
        std::vector<guid_full>      m_VirtualResources;
        std::vector<std::string>    m_Assets;
        std::vector<std::string>    m_VirtualAssets;

        XPROPERTY_DEF
        ( "depedencies", dependencies
        , obj_member<"Resources",           &dependencies::m_Resources>
        , obj_member<"VirtualResources",    &dependencies::m_VirtualResources >
        , obj_member<"Assets",              &dependencies::m_Assets>
        , obj_member<"VirtualAssets",       &dependencies::m_VirtualAssets>
        )
    };
    XPROPERTY_REG(dependencies)
}
