// Which queued compile may start (source/editor/xresource_editor_compile_schedule.h). Standalone: built and run by
// source/Editors/LevelEditor/smoke/test_compile_schedule.py. Exit 0 when every check holds.
#include "source/editor/xresource_editor_compile_schedule.h"

#include <cstdio>
#include <deque>
#include <initializer_list>
#include <set>
#include <vector>

namespace
{
    int g_Failed = 0;

    void Check(bool bOk, const char* pWhat)
    {
        if (bOk) return;
        ++g_Failed;
        std::printf("FAIL: %s\n", pWhat);
    }

    // The types as the plugins declare them (Plugin.config RunAfter): a font and a material follow the texture, an instance follows the material, a skin and an
    // animation package follow the skeleton; a static geometry follows nothing
    enum : int { texture = 1, material = 2, instance = 3, geom_static = 4, skeleton = 5, skin = 6, anim = 7, font = 8 };

    int Group(int Type)
    {
        switch (Type) { case instance: return 2; case material: case font: case skin: case anim: return 1; default: return 0; }
    }

    std::vector<int> Closure(int Type)
    {
        switch (Type)
        {
        case font:     return { texture };
        case material: return { texture };
        case instance: return { material, texture };          // transitively
        case skin:     return { skeleton };
        case anim:     return { skeleton };
        default:       return {};
        }
    }

    struct entry { int m_Type = 0; int m_Id = 0; };
    struct level { std::deque<entry> m_Deque; };

    // The queue as the manager has it: a level per run group
    std::vector<level> Queue(std::initializer_list<entry> List)
    {
        std::vector<level> Levels(3);
        for (auto E : List) Levels[Group(E.m_Type)].m_Deque.push_back(E);
        return Levels;
    }

    // Pick with the types in Running running: the id of what was picked (0: nothing), and whether the queue was empty
    int Pick(std::vector<level>& Levels, std::set<int> Running, bool* pEmpty = nullptr)
    {
        entry Out; bool bEmpty = false;
        const bool bPicked = xresource_editor::compilation::PickReady(Levels, Out, bEmpty, [](const entry& E) { return E.m_Type; }, Closure, [&](int T) { return Running.contains(T); });
        if (pEmpty) *pEmpty = bEmpty;
        return bPicked ? Out.m_Id : 0;
    }

    int Count(const std::vector<level>& Levels) { int N = 0; for (auto& L : Levels) N += static_cast<int>(L.m_Deque.size()); return N; }
}

int main()
{
    // nothing queued; something queued that cannot start is not an empty queue
    {
        auto Q = Queue({});
        bool bEmpty = false;
        Check(Pick(Q, {}, &bEmpty) == 0 && bEmpty, "an empty queue picks nothing and says it is empty");
        auto Q2 = Queue({ {font, 1} });
        Check(Pick(Q2, { texture }, &bEmpty) == 0 && !bEmpty, "a queue with something that cannot start yet is not empty");
    }

    // what the old dispatcher got wrong: a compile of a type that follows nothing starts while others compile (workers idle), whatever is queued or running
    {
        auto Q = Queue({ {geom_static, 1} });
        Check(Pick(Q, { texture, font, instance }) == 1, "a static geometry starts while textures, fonts and instances compile");
        auto Q2 = Queue({ {texture, 1}, {texture, 2}, {geom_static, 3} });
        Check(Pick(Q2, { texture }) == 1, "textures queued and running do not hold a geometry: the queue goes in order, the geometry is not stuck behind");
        Check(Pick(Q2, { texture }) == 2 && Pick(Q2, { texture }) == 3, "and the geometry is picked too, one pick at a time");
        auto Q3 = Queue({ {skeleton, 1}, {font, 2} });
        Check(Pick(Q3, { texture }) == 1, "a skeleton starts while a texture runs");
    }

    // a type waits for what it runs after: queued or running
    {
        auto Q = Queue({ {font, 1} });
        Check(Pick(Q, { texture }) == 0, "a font waits for a texture that runs");
        auto Q2 = Queue({ {font, 1}, {texture, 2} });
        Check(Pick(Q2, {}) == 2, "a font does not start while a texture is queued: the texture goes first");
        Check(Pick(Q2, {}) == 1, "and when the texture is out of the queue (and does not run) the font goes");
        auto Q3 = Queue({ {font, 1} });
        Check(Pick(Q3, { geom_static, skeleton, material }) == 1, "a font does not wait for what it does not follow");
    }

    // transitively: an instance follows the material, which follows the texture: a texture running holds it even with no material anywhere
    {
        auto Q = Queue({ {instance, 1} });
        Check(Pick(Q, { texture }) == 0, "an instance waits for a texture that runs (through the material)");
        Check(Pick(Q, { material }) == 0, "and for a material");
        Check(Pick(Q, { geom_static, font }) == 1, "and for nothing else");
    }

    // a skin and an animation package follow the skeleton, and nothing else
    {
        auto Q = Queue({ {skin, 1}, {anim, 2} });
        Check(Pick(Q, { skeleton }) == 0, "a skin and an animation package wait for a skeleton");
        Check(Pick(Q, { texture, material }) == 1 && Pick(Q, { texture, material }) == 2, "and not for textures");
    }

    // the order: the lowest level first, and inside a level the order they were queued; what is picked leaves the queue, the rest stay
    {
        auto Q = Queue({ {font, 1}, {geom_static, 2}, {skeleton, 3}, {geom_static, 4} });
        Check(Pick(Q, {}) == 2 && Count(Q) == 3, "the lowest level first (level 0 before the font of level 1), the picked one leaves the queue");
        Check(Pick(Q, {}) == 3 && Pick(Q, {}) == 4 && Pick(Q, {}) == 1 && Count(Q) == 0, "then the others in the order they were queued");
    }

    // a blocked entry does not hold the ones behind it that can go
    {
        auto Q = Queue({ {font, 1}, {geom_static, 2}, {skin, 3} });
        Check(Pick(Q, { texture }) == 2, "the font is held, the geometry behind it is not");
        Check(Pick(Q, { texture }) == 3, "nor the skin");
        Check(Pick(Q, { texture }) == 0 && Count(Q) == 1, "the font stays until the texture is done");
        Check(Pick(Q, {}) == 1, "and then goes");
    }

    if (g_Failed == 0) std::printf("all checks passed\n");
    return g_Failed == 0 ? 0 : 1;
}
