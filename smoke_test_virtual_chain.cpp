// The order the pipeline compiles virtual resources in (source/editor/xresource_editor_virtual_chain.h). Standalone: built and run by
// source/Editors/LevelEditor/smoke/test_virtual_chain.py. Exit 0 when every check holds.
#include "source/editor/xresource_editor_virtual_chain.h"

#include <array>
#include <cstdio>

namespace
{
    int g_Failed = 0;

    void Check(bool bOk, const char* pWhat)
    {
        if (bOk) return;
        ++g_Failed;
        std::printf("FAIL: %s\n", pWhat);
    }

    constexpr std::uint64_t k_Geom = 1, k_Skeleton = 2, k_Package = 3, k_Texture = 4, k_Instance = 5;

    xresource::full_guid G(std::uint64_t Instance, std::uint64_t Type) { return { xresource::instance_guid{ Instance }, xresource::type_guid{ Type } }; }

    // the run groups of the types: textures, then skeletons, then anim packages, then geometry (virtual ones run in the group of their owner anyway)
    int Group(xresource::type_guid T)
    {
        switch (T.m_Value) { case k_Texture: return 0; case k_Instance: return 1; case k_Skeleton: return 2; case k_Package: return 3; default: return 4; }
    }
}

int main()
{
    using namespace xresource_editor;

    const auto Skin     = G(0x11, k_Geom);
    const auto Skeleton = G(0x21, k_Skeleton);
    const auto Package  = G(0x31, k_Package);
    const auto Other    = G(0x12, k_Geom);

    // a virtual resource nothing lists is not compiled
    {
        virtual_chain C;
        Check(C.IsBlocked(Skeleton, false, Group), "unused: blocked");
        C.Update({}, std::array{ Skeleton, Package }, Skin);
        Check(!C.IsBlocked(Skeleton, false, Group), "listed and nothing busy: free");
        auto Dropped = C.Update(std::array{ Skeleton, Package }, {}, Skin);
        Check(Dropped.size() == 2, "the list lost both: both come back as dropped");
        Check(C.IsBlocked(Skeleton, false, Group), "dropped: unused again");
    }

    // listed twice: unused only when the last list lets go
    {
        virtual_chain C;
        C.Update({}, std::array{ Skeleton }, Skin);
        C.Update({}, std::array{ Skeleton }, Skin);
        Check(C.Update(std::array{ Skeleton }, {}, Skin).empty(), "one list still has it");
        Check(!C.IsBlocked(Skeleton, false, Group), "still used");
        Check(C.Update(std::array{ Skeleton }, {}, Skin).size() == 1, "the last list let go");
    }

    // the owner busy keeps them waiting until it ends; another owner's compile does not
    {
        virtual_chain C;
        C.Update({}, std::array{ Skeleton, Package }, Skin);
        C.Queued(Skin);
        Check(C.IsBlocked(Skeleton, false, Group) && C.IsBlocked(Package, false, Group), "owner queued: both wait");
        C.Released(Skin);
        Check(!C.IsBlocked(Skeleton, false, Group), "owner ended: the first goes");
        C.Queued(Other);
        Check(!C.IsBlocked(Skeleton, false, Group), "another owner busy: no effect");
    }

    // the order of the chain: the Skeleton before the Anim Package that reads its bones
    {
        virtual_chain C;
        C.Update({}, std::array{ Package, Skeleton }, Skin);          // listed in the wrong order on purpose
        C.Queued(Skeleton);
        Check(C.IsBlocked(Package, false, Group), "the skeleton is busy: the package waits");
        Check(!C.IsBlocked(Skeleton, false, Group), "the skeleton itself is not held by the package");
        C.Released(Skeleton);
        Check(!C.IsBlocked(Package, false, Group), "the skeleton ended: the package goes");
        C.Queued(Package);
        Check(!C.IsBlocked(Skeleton, false, Group), "a later one busy does not hold an earlier one");

        const auto List = C.Of(Skin, Group);
        Check(List.size() == 2 && List[0] == Skeleton && List[1] == Package, "the chain lists the skeleton first");
    }

    // a virtual resource owned by a virtual one goes up to the real owner
    {
        virtual_chain C;
        C.Update({}, std::array{ G(0x41, k_Instance) }, Skin);
        const auto Instance = G(0x41, k_Instance);
        C.Update({}, std::array{ G(0x51, k_Texture) }, C.RootOwner(Instance));
        Check(C.RootOwner(G(0x51, k_Texture)) == Skin, "the texture of a material instance belongs to the geometry");
        Check(C.RootOwner(Skin) == Skin, "a real resource is its own owner");
        C.Queued(Skin);
        Check(C.IsBlocked(G(0x51, k_Texture), false, Group), "the geometry is busy: its texture waits");
    }

    // Recompile All: the force of a resource that had to wait is kept for when it is free
    {
        virtual_chain C;
        C.Update({}, std::array{ Skeleton }, Skin);
        C.Queued(Skin);
        Check(C.IsBlocked(Skeleton, true, Group), "forced but its owner is busy: waits");
        C.Released(Skin);
        Check(C.TakeForce(Skeleton), "the force is still there");
        Check(!C.TakeForce(Skeleton), "and counts once");

        C.Queued(Skin);
        Check(C.IsBlocked(Skeleton, false, Group), "waits");
        C.Released(Skin);
        Check(!C.TakeForce(Skeleton), "not forced: nothing to take");
    }

    if (g_Failed == 0) std::printf("all checks passed\n");
    return g_Failed == 0 ? 0 : 1;
}
