#ifndef XRESOURCE_EDITOR_COMPILE_SCHEDULE_H
#define XRESOURCE_EDITOR_COMPILE_SCHEDULE_H
#pragma once

// Which queued compile may start now (compilation::instance's dispatcher uses it; standalone so a test can run it).
//
// The queue has a level per run group of the types, so what runs after something is further up. The old dispatcher waited for EVERY running compile before it looked at
// the next level: a geometry waited for unrelated textures, with workers idle. The rule here is what the order is for: a compile starts when no compile is queued or
// running of a type its own type runs after (transitively). Everything else starts at once, lowest level first, in the order it was queued.
#include <unordered_set>
#include <utility>
#include <vector>

namespace xresource_editor::compilation
{
    // Levels: a vector of queues, each with m_Deque (the entries, in the order they were queued). TypeOf(entry) -> the type of the resource. Closure(type) -> the types it runs
    // after (transitively). IsRunning(type) -> whether a compile of the type runs now. Takes the entry out of its queue into Out and returns true when there is one that can
    // start; bQueueEmpty tells that nothing at all is queued.
    template<class LEVELS, class ENTRY, class TYPE_OF, class CLOSURE, class IS_RUNNING>
    bool PickReady(LEVELS& Levels, ENTRY& Out, bool& bQueueEmpty, TYPE_OF&& TypeOf, CLOSURE&& Closure, IS_RUNNING&& IsRunning)
    {
        using type = decltype(TypeOf(std::declval<const ENTRY&>()));
        std::unordered_set<type> Queued;
        for (auto& Level : Levels)
            for (auto& E : Level.m_Deque) Queued.insert(TypeOf(E));
        bQueueEmpty = Queued.empty();
        if (bQueueEmpty) return false;

        for (auto& Level : Levels)
            for (auto It = Level.m_Deque.begin(); It != Level.m_Deque.end(); ++It)
            {
                bool bReady = true;
                for (const auto& Before : Closure(TypeOf(*It)))
                    if (Queued.contains(Before) || IsRunning(Before)) { bReady = false; break; }
                if (!bReady) continue;

                Out = std::move(*It);
                Level.m_Deque.erase(It);
                return true;
            }
        return false;
    }
}

#endif // XRESOURCE_EDITOR_COMPILE_SCHEDULE_H
