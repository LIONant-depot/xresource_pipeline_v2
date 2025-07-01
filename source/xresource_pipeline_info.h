namespace xresource_pipeline
{
    struct digital_rights
    {
        enum license
        { NONE
        , MIT
        , APACHE
        , GPL
        , LGPL
        , BSD
        , ZLIB
        , PUBLIC_DOMAIN
        , MAX
        };

        enum store
        { CUSTOM
        , FAB
        , UNITY
        };

        store                       m_Store             { CUSTOM };
        std::string                 m_StoreInformation  {}; 
        license                     m_License           { NONE };
        std::string                 m_LicenseInformation{};
    };

    struct info : descriptor::base
    {
        static constexpr auto       type_guid_v     = xresource::type_guid{ xcore::guid::unit("Info").m_Value };

        xresource::full_guid                m_Guid          {};
        std::string                         m_Name          {};
        std::string                         m_Comment       {};
        std::vector<xresource::full_guid>   m_RscLinks      {};

        inline              info                (void) = default;
                            info                (info&& Info) = default;
                info&       operator =          (info&& Info) = default;
        inline              info                (xresource::full_guid Guid) : m_Guid{ Guid } {}
        void                SetupFromSource     (std::string_view FileName) override {}
        void                Validate            (std::vector<std::string>& Errors) const noexcept override {}

        XPROPERTY_VDEF
        ( "Info", info
        , obj_member<"Name"
            , &info::m_Name
            , member_help<"Name of the resource, this is the name that the user will see"
            >>
        , obj_member<"Comment"
            , &info::m_Comment
            , member_help<"Comment from the user if he chooses to describe/write anything about this resource"
            >>
        , obj_scope< "Details"
            , obj_member<"Version"
                , &info::m_Version
                , member_ui_open<false>
                , member_help<"Version of the resource, which can be used for debugging"
                >>
            , obj_member< "GUID", &info::m_Guid
                , member_flags<flags::SHOW_READONLY>
                , member_help<"Unique identifier for the resource, this is how the system knows about this resource"
                >>
            , obj_member<"RscLinks"
                , &info::m_RscLinks
                , xproperty::member_flags<flags::SHOW_READONLY>
                , member_help<"This are the links associated with this resource. These links are usually to other resources, such tags, folders, etc..."
                >>
            , member_ui_open<false>
            , member_help<"Detail information that for most part the user does not need most of the time."
            >>
        )
    };
    XPROPERTY_VREG(info)

}
