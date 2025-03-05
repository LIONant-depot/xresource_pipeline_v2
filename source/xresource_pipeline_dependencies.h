namespace xresource_pipeline
{
    struct dependencies
    {
        std::vector<xresource::full_guid>       m_Resources;
        std::vector<xresource::full_guid>       m_VirtualResources;
        std::vector<std::string>                m_Assets;
        std::vector<std::string>                m_VirtualAssets;

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
