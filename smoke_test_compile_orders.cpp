// The compilation orders (source/editor/xresource_editor_compile_orders.h). Standalone: built and run by
// source/Editors/LevelEditor/smoke/test_compile_orders.py. Exit 0 when every check holds.
#include "source/editor/xresource_editor_compile_orders.h"

#include <cstdio>
#include <set>
#include <thread>

namespace
{
    int g_Failed = 0;

    void Check(bool bOk, const char* pWhat)
    {
        if (bOk) return;
        ++g_Failed;
        std::printf("FAIL: %s\n", pWhat);
    }

    struct test_log { int m_State = 0; std::uint64_t m_Order = 0; };

    xresource::full_guid G(std::uint64_t Instance, std::uint64_t Type) { return { xresource::instance_guid{ Instance }, xresource::type_guid{ Type } }; }
}

int main()
{
    using registry = xresource_editor::compilation::order_registry<test_log>;
    const auto Init = [](test_log& L, std::uint64_t Order) { L.m_State = 7; L.m_Order = Order; };

    const auto Font  = G(0x11, 1);
    const auto Atlas = G(0x21, 2);

    // an order is made with its log, which knows its number; every order is a new number
    {
        registry R;
        const auto A = R.New(Font, Init);
        const auto B = R.New(Atlas, Init);
        Check(A != 0 && B != 0 && A != B, "orders are different numbers (and never 0)");
        auto LogA = R.Find(A);
        Check(LogA && LogA->m_State == 7 && LogA->m_Order == A, "the log of the order is initialised with its number");
        Check(R.Find(B) != LogA, "each order has a log of its own");
        Check(R.FindOpen(Font) == LogA && R.FindOpen(Atlas) == R.Find(B), "the open order of a resource is found by the resource (an editor opened later finds it)");
        Check(!R.FindOpen(G(0x99, 1)), "a resource nobody queued has no order");
    }

    // an order that ends is gone: the resource has no open order, and the number finds nothing (a cancel of an old number does nothing)
    {
        registry R;
        const auto A = R.New(Font, Init);
        R.End(A);
        Check(!R.Find(A) && !R.FindOpen(Font) && R.Count() == 0, "an ended order is not found any more");
        R.End(A);
        Check(R.Count() == 0, "ending an order twice is harmless");
    }

    // a requeue made while the previous compile ends is the newer order: the editor sees the new one, and the old one ending does not take it away
    {
        registry R;
        const auto Old = R.New(Font, Init);
        const auto New = R.New(Font, Init);
        Check(R.FindOpen(Font) == R.Find(New), "the open order is the newest of the resource");
        R.End(Old);
        Check(R.FindOpen(Font) == R.Find(New) && R.Count() == 1, "the old order ending leaves the newer one");
        R.End(New);
        Check(!R.FindOpen(Font), "and when that one ends nothing is left");
    }

    // an editor that holds the log keeps it after the order ends (it shows the result)
    {
        registry R;
        const auto A = R.New(Font, Init);
        auto Held = R.Find(A);
        R.End(A);
        Check(Held && Held->m_Order == A, "the log outlives the order");
    }

    // an order is a tree: the resource and what its compile made. Nothing is done until the last of them is: the root ending alone says nothing
    {
        registry R;
        const auto Root  = R.New(Font, Init);
        const auto Child = R.New(Atlas, Root, Init);
        const auto Next  = R.New(G(0x22, 2), Child, Init);      // queued by the end of the first child: still the tree of the root
        Check(R.IsRoot(Root) && !R.IsRoot(Child) && !R.IsRoot(Next), "only the order of the resource itself is a root");

        Check(!R.End(Root), "the root's compile ending does not end the order while what it made is not done");
        Check(R.FindOpen(Font) && R.Find(Root), "the resource still has its order (an editor shows Cancel)");
        Check(!R.End(Child), "nor does the first child");
        const auto Done = R.End(Next);
        Check(Done && Done->m_Root == Root && Done->m_Guid == Font && Done->m_Outcome == xresource_editor::compilation::order_outcome::success, "the last one ends the order, said once, for the root");
        Check(Done && Done->m_Log && Done->m_Log->m_Order == Root, "the done carries the log of the root");
        Check(R.Count() == 0 && !R.FindOpen(Font), "the whole tree is removed when it is done");
    }

    // the outcome of a tree is its worst: a failed atlas fails the font's order, a cancel is better than a failure and worse than a success
    {
        using xresource_editor::compilation::order_outcome;
        registry R;
        auto Root = R.New(Font, Init); auto A = R.New(Atlas, Root, Init); auto B = R.New(G(0x22, 2), Root, Init);
        R.End(Root, order_outcome::success); R.End(A, order_outcome::failed);
        auto Done = R.End(B, order_outcome::cancelled);
        Check(Done && Done->m_Outcome == order_outcome::failed, "failed is the worst outcome");

        Root = R.New(Font, Init); A = R.New(Atlas, Root, Init);
        R.End(Root, order_outcome::success);
        Done = R.End(A, order_outcome::cancelled);
        Check(Done && Done->m_Outcome == order_outcome::cancelled, "cancelled beats success");
    }

    // a cancel of the order of the resource reaches what is not over in its tree and marks it, so what joins later does not run; a child alone is just itself
    {
        registry R;
        const auto Root  = R.New(Font, Init);
        const auto Child = R.New(Atlas, Root, Init);
        const auto Other = R.New(G(0x31, 1), Init);             // an unrelated order
        R.End(Root);
        auto Targets = R.CancelTargets(Root);
        Check(Targets.size() == 1 && Targets[0] == Child, "an order that is over is not cancelled: only what is still open in the tree");
        Check(R.IsCancelled(Root) && R.IsCancelled(Child), "the tree is marked cancelled");
        const auto Late = R.New(G(0x23, 2), Root, Init);         // queued by the chain after the cancel
        Check(R.IsCancelled(Late), "what joins the tree after the cancel is cancelled too");
        Check(!R.IsCancelled(Other), "an unrelated order is not");
        Check(R.CancelTargets(Late).size() == 1 && !R.IsCancelled(Other), "the cancel of a child is just that child");
        Check(R.CancelTargets(999).empty(), "the cancel of an order that is not there does nothing");
    }

    // what a view needs: the open orders as they are (with what the owner kept with each), and the whole tree of the one that just ended
    {
        xresource_editor::compilation::order_registry<test_log, int> R;
        const auto Root  = R.New(Font, 5, 0, Init);
        const auto Child = R.New(Atlas, 5, Root, Init);
        auto Open = R.Snapshot();
        Check(Open.size() == 2 && Open[0].m_Order == Root && Open[1].m_Parent == Root && Open[1].m_Root == Root && Open[1].m_Meta == 5, "the snapshot lists every open order with its parent, root and meta");
        R.End(Child);
        Check(R.Snapshot().size() == 2 && R.Snapshot()[1].m_bEnded, "an order that ended stays in the snapshot while its tree is open (the block shows it done)");
        const auto Done = R.End(Root);
        Check(Done && Done->m_Members.size() == 2 && Done->m_Members[0].m_Order == Root && Done->m_Members[1].m_Order == Child, "the done carries every order of the tree, for the history");
        Check(R.Snapshot().empty(), "and nothing is open after");
    }

    // a parent that is not there (its tree is gone) makes the order a root
    {
        registry R;
        const auto O = R.New(Atlas, 12345, Init);
        Check(R.IsRoot(O), "an order whose parent is not known is a root");
    }

    // orders are made from many threads (the file monitor has one per path): every number is different
    {
        registry R;
        std::set<std::uint64_t> All;
        std::mutex Lk;
        std::vector<std::thread> Threads;
        for (int t = 0; t < 8; ++t)
            Threads.emplace_back([&, t]
            {
                for (int i = 0; i < 200; ++i)
                {
                    const auto O = R.New(G(1000 + t * 200 + i, 1), Init);
                    std::scoped_lock L(Lk);
                    All.insert(O);
                }
            });
        for (auto& T : Threads) T.join();
        Check(All.size() == 1600 && R.Count() == 1600, "orders made at the same time never share a number");
    }

    if (g_Failed == 0) std::printf("all checks passed\n");
    return g_Failed == 0 ? 0 : 1;
}
