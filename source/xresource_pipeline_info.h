namespace xresource_pipeline
{
    struct info : descriptor::base
    {
        static constexpr auto       type_guid_v     = xcore::guid::unit("Info").m_Value;

        std::uint64_t               m_Guid          {};
        std::uint64_t               m_TypeGuid      {};
        std::string                 m_Name          {};
        std::string                 m_VirtualPath   {};
        const char*                 m_pTypeName     {nullptr};


        inline              info                (void) = default;
        inline              info                (std::uint64_t Guid, std::uint64_t TypeGuid, const char* pTypeName) : m_Guid{ Guid }, m_TypeGuid{ TypeGuid }, m_pTypeName{ pTypeName } {}
        void                SetupFromSource     (std::string_view FileName) override {}
        void                Validate            (std::vector<std::string>& Errors) const noexcept override {}

        XPROPERTY_VDEF
        ( "Info", info
        , obj_scope
        < "Details"
            , obj_member<"Version"
                , &info::m_Version
                , member_ui_open<false>
                , member_help<"Version of the resource, which can be used for debugging"
                >>
            , obj_member<"Guid"
                , &info::m_Guid
                , member_flags<flags::SHOW_READONLY>
                , member_ui<std::uint64_t>::drag_bar<0,0, std::numeric_limits<std::uint64_t>::max(), "%llX">
                , member_help<"64bit Unique identifier for the resource, this is how the system knows about this resource "
                              "This is part of the full 128bit which is the true unique ID of the resource"
                >>
            , obj_member<"TypeGuid"
                , &info::m_TypeGuid, member_flags<flags::SHOW_READONLY>
                , member_ui<std::uint64_t>::drag_bar<0,0, std::numeric_limits<std::uint64_t>::max(), "%llX">
                , member_help<"64bit Unique identifier for the resource type, this is how the system knows about this resource type. "
                              "This is part of the full 128bit which is the true unique ID of the resource"
                >>
            , obj_member_ro< "TypeName"
                , +[](info& O, bool, std::string& IO) { IO = O.m_pTypeName ? O.m_pTypeName : "Unkown"; }
                , xproperty::member_flags<xproperty::flags::DONT_SAVE>
                , member_help<"Name of the resource type as a string."
                >>
            , member_ui_open<false>
            , member_help<"Detail information that for most part the user does not need most of the time."
        >>
        , obj_member<"Name"
            , &info::m_Name
            , member_help<"Name of the resource, this is the name that the user will see"
            >>
        , obj_member<"VirtualPath"
            , &info::m_VirtualPath
            , member_help<"This is the path used to organize this resource in the asset browser"
            >>
        )
    };
    XPROPERTY_VREG(info)

}
