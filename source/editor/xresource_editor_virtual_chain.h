#ifndef XRESOURCE_EDITOR_VIRTUAL_CHAIN_H
#define XRESOURCE_EDITOR_VIRTUAL_CHAIN_H
#pragma once

// Who makes each virtual resource, and the order the pipeline compiles them in (library_db owns one; standalone so a test can run it).
//
// A virtual resource is made by a real resource (its owner: the resource at the top of the chain of owners, a virtual resource that lists others stays under its
// own owner). The owner's compile lists what it made; the scan and each compile that changes a list call Update. The rules of the chain:
//  - a virtual resource nothing lists (unused) is never compiled: it stays on disk with its edits
//  - it runs in the group of its owner, right after it: while the owner is queued or compiling (busy) it waits, and it queues when the owner ends
//  - the virtual resources of one owner run in the order of the run group of their type (a Skeleton before the Anim Package that reads its bones):
//    one waits while an earlier one of its owner is busy
#include "dependencies/xresource_guid/source/xresource_guid.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <span>
#include <vector>

namespace xresource_editor
{
    struct virtual_chain
    {
        struct ref
        {
            int                     m_Count = 0;        // how many lists have it
            xresource::full_guid    m_Guid  = {};
            xresource::full_guid    m_Owner = {};
        };

        // The real resource that makes this one (the resource itself when it is not a listed virtual resource)
        xresource::full_guid RootOwner( const xresource::full_guid Guid ) const noexcept
        {
            std::lock_guard Lock(m_Mutex);
            if (auto It = m_Refs.find(Guid.m_Instance.m_Value); It != m_Refs.end()) return It->second.m_Owner;
            return Guid;
        }

        // The list of one resource changed from Old to New (Owner: the real resource at the top of its chain): returns the ones nothing lists any more
        std::vector<xresource::full_guid> Update( std::span<const xresource::full_guid> Old, std::span<const xresource::full_guid> New, const xresource::full_guid Owner ) noexcept
        {
            std::lock_guard Lock(m_Mutex);
            for (auto& G : New)
            {
                auto& R = m_Refs[G.m_Instance.m_Value];
                ++R.m_Count;
                R.m_Guid  = G;
                R.m_Owner = Owner;
            }
            std::vector<xresource::full_guid> Dropped;
            for (auto& G : Old)
                if (auto It = m_Refs.find(G.m_Instance.m_Value); It != m_Refs.end() && --It->second.m_Count <= 0)
                {
                    m_Refs.erase(It);
                    Dropped.push_back(G);
                }
            return Dropped;
        }

        // The virtual resources of an owner, in the order of the chain. Group: the run group of a type (a callable: type_guid -> int)
        template<typename T_GROUP>
        std::vector<xresource::full_guid> Of( const xresource::full_guid Owner, T_GROUP&& Group ) const noexcept
        {
            std::vector<xresource::full_guid> List;
            {
                std::lock_guard Lock(m_Mutex);
                for (auto& R : m_Refs) if (R.second.m_Owner == Owner) List.push_back(R.second.m_Guid);
            }
            // (the map has no order: the guid breaks ties so that the order is the same every time)
            std::ranges::sort(List, [&](const xresource::full_guid& A, const xresource::full_guid& B)
            {
                const int GA = Group(A.m_Type), GB = Group(B.m_Type);
                return GA != GB ? GA < GB : A.m_Instance.m_Value < B.m_Instance.m_Value;
            });
            return List;
        }

        void Queued  ( const xresource::full_guid Guid ) const noexcept { std::lock_guard Lock(m_Mutex); m_Busy.insert(Guid.m_Instance.m_Value); }
        void Released( const xresource::full_guid Guid ) const noexcept { std::lock_guard Lock(m_Mutex); m_Busy.erase (Guid.m_Instance.m_Value); }

        // A virtual resource that must not be queued now: unused, its owner is busy, or an earlier one of its owner is. bForce (Recompile All) is kept for
        // when it is free again (TakeForce)
        template<typename T_GROUP>
        bool IsBlocked( const xresource::full_guid Guid, const bool bForce, T_GROUP&& Group ) const noexcept
        {
            std::lock_guard Lock(m_Mutex);

            const auto It = m_Refs.find(Guid.m_Instance.m_Value);
            if (It == m_Refs.end()) return true;

            bool bWait = m_Busy.contains(It->second.m_Owner.m_Instance.m_Value);
            if (!bWait)
            {
                const int MyGroup = Group(Guid.m_Type);
                for (auto& R : m_Refs)
                    if (R.second.m_Owner == It->second.m_Owner && R.first != Guid.m_Instance.m_Value && m_Busy.contains(R.first) && Group(R.second.m_Guid.m_Type) < MyGroup)
                    {
                        bWait = true;
                        break;
                    }
            }

            if (bWait && bForce) m_ForceWhenFree.insert(Guid.m_Instance.m_Value);
            return bWait;
        }

        // Whether something lists it (it is not unused)
        bool IsListed( const xresource::full_guid Guid ) const noexcept
        {
            std::lock_guard Lock(m_Mutex);
            return m_Refs.contains(Guid.m_Instance.m_Value);
        }

        // It was forced while it had to wait (and is free now): the force counts once
        bool TakeForce( const xresource::full_guid Guid ) const noexcept
        {
            std::lock_guard Lock(m_Mutex);
            return m_ForceWhenFree.erase(Guid.m_Instance.m_Value) != 0;
        }

        mutable std::mutex                              m_Mutex;
        std::unordered_map<std::uint64_t, ref>          m_Refs;                 // by instance value
        mutable std::unordered_set<std::uint64_t>       m_Busy;                 // queued or compiling
        mutable std::unordered_set<std::uint64_t>       m_ForceWhenFree;
    };
}

#endif
