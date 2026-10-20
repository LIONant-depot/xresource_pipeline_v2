#ifndef XRESOURCE_EDITOR_COMPILE_ORDERS_H
#define XRESOURCE_EDITOR_COMPILE_ORDERS_H
#pragma once

// The compilation orders (compilation::instance owns one; standalone so a test can run it).
//
// An order is made the moment a resource is queued. The number is what an editor holds to ask for it to be cancelled; the log (whatever the owner makes of LOG)
// is the state of that resource's own compile. The manager is the one that knows the orders, so an editor that was closed and opened again asks it for the order of
// its resource and finds it still there.
//
// An order is a tree: the resource (the root) and what its compile made - the virtual resources it emitted (a font's atlas, a geometry's materials and textures),
// which are queued while the root's compile ends, or while an earlier one of them ends (their order is a child of the order that queued them). The tree is over
// when every order in it is: only then is it removed, and only then is it said to be done, with the worst outcome of its orders (failed, then cancelled, then
// success). Resources that merely depend on the root are not part of it: they have orders of their own.
#include "dependencies/xresource_guid/source/xresource_guid.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace xresource_editor::compilation
{
    enum class order_outcome : std::uint8_t { success, cancelled, failed };

    // META: what the owner wants to keep with each order (the library of the resource)
    template<class LOG, class META = int>
    struct order_registry
    {
        struct info
        {
            std::uint64_t           m_Order     = 0;
            std::uint64_t           m_Parent    = 0;        // the order that queued this one (0: nobody, the order is a root)
            std::uint64_t           m_Root      = 0;        // the root of the tree it is in (its own number for a root)
            xresource::full_guid    m_Guid      = {};
            META                    m_Meta      = {};
            std::shared_ptr<LOG>    m_Log;
            bool                    m_bEnded    = false;
            bool                    m_bCancel   = false;    // roots: the tree was cancelled, what is queued in it from now on must not run
            order_outcome           m_Outcome   = order_outcome::success;
        };

        struct tree_done
        {
            std::uint64_t           m_Root = 0;
            xresource::full_guid    m_Guid = {};            // the resource of the root
            order_outcome           m_Outcome = order_outcome::success;
            std::shared_ptr<LOG>    m_Log;                  // the log of the root
            std::vector<info>       m_Members;              // every order of the tree, oldest first (the root is the one whose m_Root is its own number)
        };

        // Init(LOG&, Order) puts the order in the new log (and its first state). Parent: the order doing the queuing (0 when it is not part of one)
        template<class INIT>
        std::uint64_t New(const xresource::full_guid& Guid, const META& Meta, std::uint64_t Parent, INIT&& Init) noexcept
        {
            auto Log = std::make_shared<LOG>();
            std::scoped_lock Lk(m_Mutex);
            const auto Order = m_Next++;
            Init(*Log, Order);
            info I;
            I.m_Order  = Order;
            I.m_Guid   = Guid;
            I.m_Meta   = Meta;
            I.m_Log    = std::move(Log);
            I.m_Root   = Order;
            if (Parent)
                if (const auto* P = FindInfo(Parent)) { I.m_Parent = Parent; I.m_Root = P->m_Root; }
            m_List.push_back(std::move(I));
            return Order;
        }

        template<class INIT>
        std::uint64_t New(const xresource::full_guid& Guid, std::uint64_t Parent, INIT&& Init) noexcept { return New(Guid, META{}, Parent, std::forward<INIT>(Init)); }

        template<class INIT>
        std::uint64_t New(const xresource::full_guid& Guid, INIT&& Init) noexcept { return New(Guid, META{}, 0, std::forward<INIT>(Init)); }

        std::shared_ptr<LOG> Find(std::uint64_t Order) noexcept
        {
            std::scoped_lock Lk(m_Mutex);
            const auto* I = FindInfo(Order);
            return I ? I->m_Log : nullptr;
        }

        // The newest order of the resource that is in a tree that is not over (its own compile may be over: the log says)
        std::shared_ptr<LOG> FindOpen(const xresource::full_guid& Guid) noexcept
        {
            std::scoped_lock Lk(m_Mutex);
            for (auto it = m_List.rbegin(); it != m_List.rend(); ++it) if (it->m_Guid == Guid) return it->m_Log;
            return {};
        }

        bool IsRoot(std::uint64_t Order) noexcept
        {
            std::scoped_lock Lk(m_Mutex);
            const auto* I = FindInfo(Order);
            return I && I->m_Root == Order;
        }

        // The tree of this order was cancelled: whatever of it is about to run must not (a worker asks when it takes an order)
        bool IsCancelled(std::uint64_t Order) noexcept
        {
            std::scoped_lock Lk(m_Mutex);
            const auto* I = FindInfo(Order);
            if (!I) return false;
            const auto* R = FindInfo(I->m_Root);
            return R && R->m_bCancel;
        }

        // What a cancel of this order reaches: for a root, every order of its tree that is not over (and the tree is marked, so what joins it later is cancelled
        // too); for any other order, just that one. An order that is over is not listed.
        std::vector<std::uint64_t> CancelTargets(std::uint64_t Order) noexcept
        {
            std::scoped_lock Lk(m_Mutex);
            std::vector<std::uint64_t> Out;
            auto* I = FindInfo(Order);
            if (!I) return Out;
            if (I->m_Root == Order)
            {
                I->m_bCancel = true;
                for (auto& O : m_List) if (O.m_Root == Order && !O.m_bEnded) Out.push_back(O.m_Order);
            }
            else if (!I->m_bEnded) Out.push_back(Order);
            return Out;
        }

        // The order is over. When it was the last of its tree, the tree is removed and returned.
        std::optional<tree_done> End(std::uint64_t Order, order_outcome Outcome = order_outcome::success) noexcept
        {
            std::scoped_lock Lk(m_Mutex);
            auto* I = FindInfo(Order);
            if (!I) return std::nullopt;
            I->m_bEnded  = true;
            I->m_Outcome = Outcome;

            const auto Root = I->m_Root;
            for (const auto& O : m_List) if (O.m_Root == Root && !O.m_bEnded) return std::nullopt;

            tree_done Done;
            Done.m_Root = Root;
            for (const auto& O : m_List)
            {
                if (O.m_Root != Root) continue;
                Done.m_Members.push_back(O);
                if (O.m_Order == Root) { Done.m_Guid = O.m_Guid; Done.m_Log = O.m_Log; }
                if (O.m_Outcome == order_outcome::failed || (O.m_Outcome == order_outcome::cancelled && Done.m_Outcome == order_outcome::success)) Done.m_Outcome = O.m_Outcome;
            }
            std::erase_if(m_List, [&](const info& O) { return O.m_Root == Root; });
            return Done;
        }

        // The orders that are not over, a copy: every tree (oldest order first)
        std::vector<info> Snapshot() noexcept { std::scoped_lock Lk(m_Mutex); return m_List; }

        std::size_t Count() noexcept { std::scoped_lock Lk(m_Mutex); return m_List.size(); }

    private:
        info* FindInfo(std::uint64_t Order) noexcept
        {
            for (auto& O : m_List) if (O.m_Order == Order) return &O;
            return nullptr;
        }

        std::mutex          m_Mutex;
        std::vector<info>   m_List;         // oldest first
        std::uint64_t       m_Next = 1;
    };
}

#endif // XRESOURCE_EDITOR_COMPILE_ORDERS_H
