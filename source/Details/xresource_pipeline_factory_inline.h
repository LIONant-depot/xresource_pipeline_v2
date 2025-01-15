namespace xresource_pipeline
{
    struct info_factory final : xresource_pipeline::factory_base
    {
        using xresource_pipeline::factory_base::factory_base;

        std::unique_ptr<xresource_pipeline::descriptor::base> CreateDescriptor(void) const noexcept override
        {
            return std::make_unique<info>();
        };

        std::uint64_t ResourceTypeGUID(void) const noexcept override
        {
            return info::type_guid_v;
        }

        const char* ResourceTypeName(void) const noexcept override
        {
            return "Info";
        }

        const xproperty::type::object& ResourceXPropertyObject(void) const noexcept override
        {
            return *xproperty::getObjectByType<info>();
        }
    };

    inline static info_factory g_Factory{};

}