namespace xresource_pipeline
{
    struct factory_base
    {
        inline static const factory_base* s_pHead = nullptr;
        const factory_base* const         m_pNext;
        inline static const factory_base* Find(std::string_view ResourceTypeName) noexcept
        {
            for (auto p = s_pHead; p; p = p->m_pNext) if (p->ResourceTypeName() == ResourceTypeName) return p;
            return nullptr;
        }
        inline static const factory_base* Find(xresource::type_guid ResourceTypeGUID) noexcept
        {
            for (auto p = s_pHead; p; p = p->m_pNext) if (p->ResourceTypeGUID() == ResourceTypeGUID) return p;
            return nullptr;
        }
                                                    factory_base            (void) noexcept : m_pNext{ s_pHead } { s_pHead = this; }
        virtual                                    ~factory_base            (void) noexcept = default;
        virtual std::unique_ptr<descriptor::base>   CreateDescriptor        (void) const noexcept = 0;
        virtual xresource::type_guid                ResourceTypeGUID        (void) const noexcept = 0;
        virtual const char*                         ResourceTypeName        (void) const noexcept = 0;
        virtual const xproperty::type::object&      ResourceXPropertyObject (void) const noexcept = 0;

        inline info                             CreateInfo              ( xresource::instance_guid GUID = {}) const noexcept
        {
            if (GUID.empty())
            {
                GUID = xresource::instance_guid::GenerateGUIDCopy();
            }
            else
            {
                // All resources have to have bit 1 set as true
                assert(GUID.isValid());
            }

            return info{ {GUID, ResourceTypeGUID()} };
        }
    };
}