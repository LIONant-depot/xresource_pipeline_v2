#ifndef XRESOURCE_EDITOR_COMMANDS_LIBRARY_DEPENDENCY_H
#define XRESOURCE_EDITOR_COMMANDS_LIBRARY_DEPENDENCY_H
#pragma once

// AddLibraryDependency / RemoveLibraryDependency / CreateLibrary - explicit library m_ParentLibraries
// authoring, a near-direct port of the scene dependency commands's own shape onto xresource_editor::library (see
// the "Multi-library project model" plan section). Unlike a scene dependency (guid-only - every scene
// already lives under the SAME project's own scene folder), a library dependency can be in a
// completely different depot, so locating one that isn't already loaded this session needs a Path,
// carried as text on -ParentPath.
//
// Cycle detection (WouldCreateLibraryDependencyCycle) is scoped to currently-LOADED libraries only -
// unlike a scene's fixed-folder-derivable Descriptor path, a library's Path lives only inside another
// library's own m_ParentLibraries entry pointing at it, so there is no generic way to read an
// arbitrary UNLOADED library's own config from just its guid. Documented limitation, not silently
// hidden - correct for the common case (every library actually in play this session is loaded), which
// is the only case this system can support until libraries carry a global path index of their own.
//
// Removal-safety: refuses to remove a dependency edge when doing so would orphan a live cross-
// library resource reference - direct user requirement ("to remove the dependency you have to make
// sure no resource of the parent library has a reference anywhere in the tree of dependencies that
// you are removing"), mirroring WhyCannotRemoveSceneDependency/CollectLostParentsOnRemove
// (xscene_dependencies.h) exactly: compute which libraries become unreachable from Owner once
// DirectParent is cut (DirectParent itself, plus anything only reachable through it), then scan
// every resource Owner itself owns for a m_Dependencies.m_Resources entry that resolves (via
// library_mgr::m_RscToLibraryMap) to one of those now-unreachable libraries. No entity-reference-
// style "-ClearRefs 1, null them and remove anyway" escape hatch here (unlike scenes) - there is no
// live in-memory field to null the way an entity reference can be; the resource itself would need to
// be re-pointed or deleted first, an explicit, separate action.
//
// CreateLibrary is a query_command_base, not undo-tracked: unlike CreateAsset (whose Undo can at
// least MoveToTrash what it made), there is no per-library unload/trash primitive in this system at
// all yet, so there is no honest "undo" target for a brand-new library - same reasoning EmptyTrashcan
// already established for "real/irreversible, stays outside the undo system."
#include "dependencies/xresource_pipeline_v2/source/editor/xresource_editor_command_guids.h"
#include "dependencies/xundo/source/xundo_system.h"
#include "dependencies/xeditor/include/xeditor/serialize.h"

namespace xresource_editor::commands
{
    inline std::string  EncodeLibraryPath(const std::wstring& Path) noexcept { return xeditor::Quote(xstrtool::To(Path)); }
    inline std::wstring DecodeLibraryPath(const std::string& Encoded) noexcept { return xstrtool::To(Encoded); }

    // Graph-walking primitives (CollectTransitiveLibraryParents/IsLibraryLegalReferenceTarget) now
    // live on library_mgr itself (xresource_editor_asset_mgr.h) - shared with the generic Asset Browser UI's own
    // picker filtering, which cannot depend on this command file. This file's own commands
    // just call xresource_editor::g_LibMgr.CollectTransitiveLibraryParents(...) directly below.

    // Would adding "NewDependency" to Candidate's own m_ParentLibraries close a cycle? True iff
    // Candidate is already (transitively) reachable FROM NewDependency by walking m_ParentLibraries
    // edges - same check shape as WouldCreateDependencyCycle (xscene_dependencies.h), scene case.
    inline bool WouldCreateLibraryDependencyCycle(xresource_editor::library::guid Candidate, xresource_editor::library::guid NewDependency) noexcept
    {
        if (Candidate == NewDependency) return true;
        std::vector<xresource_editor::library::guid> Reachable;
        xresource_editor::g_LibMgr.CollectTransitiveLibraryParents(std::vector<xresource_editor::library::guid>{ NewDependency }, xresource_editor::library::guid{}, Reachable);
        return std::find(Reachable.begin(), Reachable.end(), Candidate) != Reachable.end();
    }

    // Libraries that become unreachable from Owner once DirectParent is removed from Owner's own
    // m_ParentLibraries (DirectParent itself plus anything only reachable through it) - same
    // Before/After transitive-closure diff as CollectLostParentsOnRemove's own scene-case shape.
    inline void CollectLostLibrariesOnRemove(xresource_editor::library::guid OwnerGuid, xresource_editor::library::guid DirectParent, std::vector<xresource_editor::library::guid>& OutLost) noexcept
    {
        std::vector<xresource_editor::library::guid> DirectParents;
        xresource_editor::g_LibMgr.m_mLibraryDB.FindAsReadOnly(OwnerGuid, [&](const std::unique_ptr<xresource_editor::library_db>& DB)
        {
            for (auto& P : DB->m_Library.m_ParentLibraries)
                DirectParents.push_back(P.m_GUID);
        });

        std::vector<xresource_editor::library::guid> Before, After;
        xresource_editor::g_LibMgr.CollectTransitiveLibraryParents(DirectParents, xresource_editor::library::guid{}, Before);
        xresource_editor::g_LibMgr.CollectTransitiveLibraryParents(DirectParents, DirectParent, After);

        OutLost.clear();
        for (auto& G : Before)
            if (std::find(After.begin(), After.end(), G) == After.end())
                OutLost.push_back(G);
    }

    // Refuses removal when Owner's OWN resources hold a live m_Dependencies.m_Resources reference
    // into a library that CollectLostLibrariesOnRemove says would become unreachable. Resolves each
    // referenced resource's owning library via library_mgr::m_RscToLibraryMap - a bare
    // xresource::full_guid carries no library identity of its own, that map is the only place this
    // question can be answered. A reference to a resource m_RscToLibraryMap has no entry for yet
    // (e.g. that resource was never scanned) is silently skipped, not treated as a hit - matches this
    // system's existing "best-effort, never block on an admittedly-incomplete index" posture.
    inline std::string WhyCannotRemoveLibraryDependency(xresource_editor::library::guid OwnerGuid, xresource_editor::library::guid DirectParent) noexcept
    {
        std::vector<xresource_editor::library::guid> Lost;
        CollectLostLibrariesOnRemove(OwnerGuid, DirectParent, Lost);
        if (Lost.empty()) return {};

        auto IsLost = [&](xresource_editor::library::guid G) noexcept
        {
            return std::find(Lost.begin(), Lost.end(), G) != Lost.end();
        };

        int HitCount = 0;
        std::string FirstHitName;

        const bool bOwnerLoaded = xresource_editor::g_LibMgr.m_mLibraryDB.FindAsReadOnly(OwnerGuid, [&](const std::unique_ptr<xresource_editor::library_db>& DB)
        {
            for (auto& TypeEntry : DB->m_InfoByTypeDataBase)
            {
                for (auto& NodeEntry : TypeEntry.second->m_InfoDataBase)
                {
                    auto& Node = NodeEntry.second;
                    for (auto& Dep : Node.m_Dependencies.m_Resources)
                    {
                        xresource_editor::library::guid OwningLib{};
                        const bool bResolved = xresource_editor::g_LibMgr.m_RscToLibraryMap.FindAsReadOnly(Dep, [&](const xresource_editor::library::guid& L) { OwningLib = L; });
                        if (bResolved && IsLost(OwningLib))
                        {
                            ++HitCount;
                            if (FirstHitName.empty()) FirstHitName = Node.m_Info.m_Name;
                        }
                    }
                }
            }
        });
        if (!bOwnerLoaded) return "RemoveLibraryDependency: owning library is not loaded";
        if (HitCount == 0) return {};

        return std::format("RemoveLibraryDependency: {} resource reference(s) would be orphaned (e.g. \"{}\") - repoint or remove them first", HitCount, FirstHitName);
    }

    //================================================================================================
    // AddLibraryDependency - Library gains Parent as a direct m_ParentLibraries entry (undoable),
    // persisted to Library.config.txt immediately, and Parent is brought resident this session as a
    // dependent load if it wasn't already (mirrors EnsureLibraryLoaded's own dependency-recursion
    // semantics - bExplicitRequest=false).
    // Usage: AddLibraryDependency -Library hexguid -Parent hexguid [-ParentPath text]
    //================================================================================================
    struct add_library_dependency_cmd : xundo::command_base
    {
        add_library_dependency_cmd(xundo::system& System, void* pDataBase) noexcept : xundo::command_base(System, "AddLibraryDependency", pDataBase) { RegisterArguments(); }
        const char* getCommandHelp() const noexcept override
        {
            return "Adds an explicit library dependency (ParentLibraries), persists it, and loads the dependency into this session if needed. Refuses cycles. Usage: AddLibraryDependency -Library hexguid -Parent hexguid [-ParentPath text] (ParentPath required only the first time a not-yet-loaded library is referenced)";
        }
        void RegisterArguments() noexcept override
        {
            m_hLibrary    = m_Parser.addOption("Library",    "Owning library instance guid, 16 hex digits",                              true,  1);
            m_hParent     = m_Parser.addOption("Parent",     "Dependency library instance guid, 16 hex digits",                          true,  1);
            m_hParentPath = m_Parser.addOption("ParentPath", "Dependency library's root path - required if not already loaded",  false, 1);
        }

        std::string Redo() noexcept override
        {
            auto LibraryArg = m_Parser.getOptionArgAs<std::string>(m_hLibrary, 0);
            auto ParentArg  = m_Parser.getOptionArgAs<std::string>(m_hParent, 0);
            if (std::holds_alternative<xerr>(LibraryArg) || std::holds_alternative<xerr>(ParentArg))
                return "AddLibraryDependency: bad arguments";

            const auto LibraryGuid = xresource_editor::commands::ParseLibraryGuid(std::get<std::string>(LibraryArg));
            const auto ParentGuid  = xresource_editor::commands::ParseLibraryGuid(std::get<std::string>(ParentArg));
            if (LibraryGuid == ParentGuid) return "AddLibraryDependency: a library cannot depend on itself";

            bool bLibraryLoaded = false;
            bool bAlreadyPresent = false;
            xresource_editor::g_LibMgr.m_mLibraryDB.FindAsReadOnly(LibraryGuid, [&](const std::unique_ptr<xresource_editor::library_db>& DB)
            {
                bLibraryLoaded = true;
                for (auto& P : DB->m_Library.m_ParentLibraries)
                    if (P.m_GUID == ParentGuid) { bAlreadyPresent = true; break; }
            });
            if (!bLibraryLoaded) return "AddLibraryDependency: owning library is not loaded";
            if (bAlreadyPresent) return {};

            if (WouldCreateLibraryDependencyCycle(LibraryGuid, ParentGuid))
                return "AddLibraryDependency: would create a circular library dependency";

            // Resolve the dependency's Path - prefer the already-loaded copy, else the caller-supplied -ParentPath.
            std::wstring ParentPath;
            bool bParentLoaded = xresource_editor::g_LibMgr.m_mLibraryDB.FindAsReadOnly(ParentGuid, [&](const std::unique_ptr<xresource_editor::library_db>& DB)
            {
                ParentPath = DB->m_Library.m_Path;
            });
            if (!bParentLoaded)
            {
                auto ParentPathArg = m_Parser.getOptionArgAs<std::string>(m_hParentPath, 0);
                if (std::holds_alternative<xerr>(ParentPathArg))
                    return "AddLibraryDependency: dependency is not loaded this session - -ParentPath is required";
                ParentPath = DecodeLibraryPath(std::get<std::string>(ParentPathArg));
            }

            // Bring the dependency resident (dependent load, not explicit) BEFORE recording the edge,
            // so a failure to load never leaves a dangling edge behind.
            xresource_editor::library::guid LoadedGuid{};
            if (auto Err = xresource_editor::g_LibMgr.EnsureLibraryLoaded(ParentPath, /*bExplicitRequest*/ false, /*bIsRootProject*/ false, LoadedGuid); Err)
                return std::format("AddLibraryDependency: failed to load dependency: {}", Err.getMessage());
            if (LoadedGuid != ParentGuid)
                return "AddLibraryDependency: -Parent guid does not match the library found at -ParentPath";

            xerr SaveErr;
            xresource_editor::g_LibMgr.m_mLibraryDB.FindAsWrite(LibraryGuid, [&](std::unique_ptr<xresource_editor::library_db>& DB)
            {
                xresource_editor::library Stub;
                Stub.m_GUID = ParentGuid;
                Stub.m_Path = ParentPath;
                DB->m_Library.m_ParentLibraries.push_back(std::move(Stub));
                SaveErr = xresource_editor::g_LibMgr.SaveLibraryConfig(DB->m_Library);
            });
            if (SaveErr) return std::format("AddLibraryDependency: {}", SaveErr.getMessage());
            return {};
        }

        void BackupCurrenState(xundo::undo_file& File) noexcept override
        {
            auto LibraryArg = m_Parser.getOptionArgAs<std::string>(m_hLibrary, 0);
            auto ParentArg  = m_Parser.getOptionArgAs<std::string>(m_hParent, 0);
            const std::uint64_t Library = std::holds_alternative<xerr>(LibraryArg) ? 0 : std::strtoull(std::get<std::string>(LibraryArg).c_str(), nullptr, 16);
            const std::uint64_t Parent  = std::holds_alternative<xerr>(ParentArg)  ? 0 : std::strtoull(std::get<std::string>(ParentArg).c_str(), nullptr, 16);
            std::uint32_t bWasPresent = 0;
            if (Library && Parent)
            {
                xresource_editor::g_LibMgr.m_mLibraryDB.FindAsReadOnly(xresource_editor::library::guid{ .m_Instance = { Library } }, [&](const std::unique_ptr<xresource_editor::library_db>& DB)
                {
                    for (auto& P : DB->m_Library.m_ParentLibraries)
                        if (P.m_GUID.m_Instance.m_Value == Parent) { bWasPresent = 1; break; }
                });
            }
            File.Write(Library);
            File.Write(Parent);
            File.Write(bWasPresent);
        }

        void Undo(xundo::undo_file& File) noexcept override
        {
            std::uint64_t Library = 0; File.Read(Library);
            std::uint64_t Parent = 0; File.Read(Parent);
            std::uint32_t bWasPresent = 0; File.Read(bWasPresent);
            if (bWasPresent) return;

            const auto LibraryGuid = xresource_editor::library::guid{ .m_Instance = { Library } };
            const auto ParentGuid  = xresource_editor::library::guid{ .m_Instance = { Parent } };

            xerr SaveErr;
            xresource_editor::g_LibMgr.m_mLibraryDB.FindAsWrite(LibraryGuid, [&](std::unique_ptr<xresource_editor::library_db>& DB)
            {
                auto& List = DB->m_Library.m_ParentLibraries;
                if (auto It = std::find_if(List.begin(), List.end(), [&](const xresource_editor::library& L) { return L.m_GUID == ParentGuid; }); It != List.end())
                    List.erase(It);
                SaveErr = xresource_editor::g_LibMgr.SaveLibraryConfig(DB->m_Library);
            });

            // Residency bookkeeping only - no per-library unload exists yet (see this file's own top
            // comment), so this purely keeps the counter honest, it never actually frees anything.
            xresource_editor::g_LibMgr.m_mLibraryDB.FindAsWrite(ParentGuid, [&](std::unique_ptr<xresource_editor::library_db>& DB)
            {
                if (DB->m_DependentLibraryCount > 0) DB->m_DependentLibraryCount--;
            });
        }

        xcmdline::parser::handle m_hLibrary, m_hParent, m_hParentPath;
    };

    //================================================================================================
    // RemoveLibraryDependency - erases Parent from Library.m_ParentLibraries (undoable), persisted
    // immediately. Refuses when Owner's own resources hold a live reference into whatever would
    // become unreachable - see this file's own top comment on removal-safety.
    // Usage: RemoveLibraryDependency -Library hexguid -Parent hexguid
    //================================================================================================
    struct remove_library_dependency_cmd : xundo::command_base
    {
        remove_library_dependency_cmd(xundo::system& System, void* pDataBase) noexcept : xundo::command_base(System, "RemoveLibraryDependency", pDataBase) { RegisterArguments(); }
        const char* getCommandHelp() const noexcept override
        {
            return "Removes an explicit library dependency. Refuses if any of the owning library's own resources still reference something that would become unreachable. Usage: RemoveLibraryDependency -Library hexguid -Parent hexguid";
        }
        void RegisterArguments() noexcept override
        {
            m_hLibrary = m_Parser.addOption("Library", "Owning library instance guid, 16 hex digits",      true, 1);
            m_hParent  = m_Parser.addOption("Parent",  "Dependency library instance guid, 16 hex digits",  true, 1);
        }

        std::string Redo() noexcept override
        {
            auto LibraryArg = m_Parser.getOptionArgAs<std::string>(m_hLibrary, 0);
            auto ParentArg  = m_Parser.getOptionArgAs<std::string>(m_hParent, 0);
            if (std::holds_alternative<xerr>(LibraryArg) || std::holds_alternative<xerr>(ParentArg))
                return "RemoveLibraryDependency: bad arguments";

            const auto LibraryGuid = xresource_editor::commands::ParseLibraryGuid(std::get<std::string>(LibraryArg));
            const auto ParentGuid  = xresource_editor::commands::ParseLibraryGuid(std::get<std::string>(ParentArg));

            if (auto Why = WhyCannotRemoveLibraryDependency(LibraryGuid, ParentGuid); !Why.empty())
                return Why;

            bool bFound = false;
            xerr SaveErr;
            xresource_editor::g_LibMgr.m_mLibraryDB.FindAsWrite(LibraryGuid, [&](std::unique_ptr<xresource_editor::library_db>& DB)
            {
                auto& List = DB->m_Library.m_ParentLibraries;
                if (auto It = std::find_if(List.begin(), List.end(), [&](const xresource_editor::library& L) { return L.m_GUID == ParentGuid; }); It != List.end())
                {
                    List.erase(It);
                    bFound = true;
                    SaveErr = xresource_editor::g_LibMgr.SaveLibraryConfig(DB->m_Library);
                }
            });
            if (!bFound) return "RemoveLibraryDependency: parent is not a dependency";
            if (SaveErr) return std::format("RemoveLibraryDependency: {}", SaveErr.getMessage());

            xresource_editor::g_LibMgr.m_mLibraryDB.FindAsWrite(ParentGuid, [&](std::unique_ptr<xresource_editor::library_db>& DB)
            {
                if (DB->m_DependentLibraryCount > 0) DB->m_DependentLibraryCount--;
            });
            return {};
        }

        void BackupCurrenState(xundo::undo_file& File) noexcept override
        {
            auto LibraryArg = m_Parser.getOptionArgAs<std::string>(m_hLibrary, 0);
            auto ParentArg  = m_Parser.getOptionArgAs<std::string>(m_hParent, 0);
            const std::uint64_t Library = std::holds_alternative<xerr>(LibraryArg) ? 0 : std::strtoull(std::get<std::string>(LibraryArg).c_str(), nullptr, 16);
            const std::uint64_t Parent  = std::holds_alternative<xerr>(ParentArg)  ? 0 : std::strtoull(std::get<std::string>(ParentArg).c_str(), nullptr, 16);
            std::uint32_t Index = 0;
            std::wstring ParentPath;
            if (Library && Parent)
            {
                xresource_editor::g_LibMgr.m_mLibraryDB.FindAsReadOnly(xresource_editor::library::guid{ .m_Instance = { Library } }, [&](const std::unique_ptr<xresource_editor::library_db>& DB)
                {
                    auto& List = DB->m_Library.m_ParentLibraries;
                    for (std::size_t i = 0; i < List.size(); ++i)
                        if (List[i].m_GUID.m_Instance.m_Value == Parent) { Index = static_cast<std::uint32_t>(i); ParentPath = List[i].m_Path; break; }
                });
            }
            File.Write(Library);
            File.Write(Parent);
            File.Write(Index);
            xeditor::WriteString(File, xstrtool::To(ParentPath));
        }

        void Undo(xundo::undo_file& File) noexcept override
        {
            std::uint64_t Library = 0; File.Read(Library);
            std::uint64_t Parent = 0; File.Read(Parent);
            std::uint32_t Index = 0; File.Read(Index);
            const std::wstring ParentPath = xstrtool::To(xeditor::ReadString(File));

            const auto LibraryGuid = xresource_editor::library::guid{ .m_Instance = { Library } };
            const auto ParentGuid  = xresource_editor::library::guid{ .m_Instance = { Parent } };

            xerr SaveErr;
            xresource_editor::g_LibMgr.m_mLibraryDB.FindAsWrite(LibraryGuid, [&](std::unique_ptr<xresource_editor::library_db>& DB)
            {
                auto& List = DB->m_Library.m_ParentLibraries;
                if (std::find_if(List.begin(), List.end(), [&](const xresource_editor::library& L) { return L.m_GUID == ParentGuid; }) == List.end())
                {
                    xresource_editor::library Stub;
                    Stub.m_GUID = ParentGuid;
                    Stub.m_Path = ParentPath;
                    const auto Idx = std::min<std::size_t>(Index, List.size());
                    List.insert(List.begin() + static_cast<std::ptrdiff_t>(Idx), std::move(Stub));
                }
                SaveErr = xresource_editor::g_LibMgr.SaveLibraryConfig(DB->m_Library);
            });

            xresource_editor::g_LibMgr.m_mLibraryDB.FindAsWrite(ParentGuid, [&](std::unique_ptr<xresource_editor::library_db>& DB)
            {
                DB->m_DependentLibraryCount++;
            });
        }

        xcmdline::parser::handle m_hLibrary, m_hParent;
    };

    //================================================================================================
    // CreateLibrary - writes a brand-new, standalone Library.config.txt (freshly generated GUID via
    // instance_guid::GenerateGUID(), which always sets the odd/"plain value" tag bit - never the
    // pointer-tag-colliding kind a hand-picked hex guid can accidentally produce) and loads it into
    // this session as its OWN independent library - not added as a dependency or project member of
    // anything. Not undo-tracked (query_command_base) - see this file's own top comment for why.
    // Usage: CreateLibrary -Path text
    //================================================================================================
    struct create_library_query_cmd : xundo::query_command_base
    {
        create_library_query_cmd(xundo::system& System, void* pDataBase) noexcept : xundo::query_command_base(System, "CreateLibrary", pDataBase) { RegisterArguments(); }
        const char* getCommandHelp() const noexcept override
        {
            return "Creates a brand-new, standalone library on disk and loads it into this session, independent of any other library. Not undoable. Usage: CreateLibrary -Path text";
        }
        void RegisterArguments() noexcept override
        {
            m_hPath = m_Parser.addOption("Path", "New library's root folder", true, 1);
        }

        std::string Query() noexcept override
        {
            auto PathArg = m_Parser.getOptionArgAs<std::string>(m_hPath, 0);
            if (std::holds_alternative<xerr>(PathArg)) return "CreateLibrary: bad arguments";

            const auto Path = DecodeLibraryPath(std::get<std::string>(PathArg));
            if (Path.empty()) return "CreateLibrary: empty path";

            const auto ConfigPath = std::format(L"{}\\Project.config\\Library.config.txt", Path);
            if (std::filesystem::exists(ConfigPath))
                return "CreateLibrary: a Library.config.txt already exists at this path";

            xresource_editor::library Lib;
            Lib.m_GUID.m_Instance.GenerateGUID();
            Lib.m_Path = Path;

            xresource_editor::create_directory_path(std::format(L"{}\\Project.config", Path));
            if (auto Err = xresource_editor::g_LibMgr.SaveLibraryConfig(Lib); Err)
                return std::format("CreateLibrary: {}", Err.getMessage());

            xresource_editor::library::guid OutGuid{};
            if (auto Err = xresource_editor::g_LibMgr.EnsureLibraryLoaded(Path, /*bExplicitRequest*/ true, /*bIsRootProject*/ false, OutGuid); Err)
                return std::format("CreateLibrary: written to disk but failed to load: {}", Err.getMessage());

            return xresource_editor::commands::FormatLibraryGuid(OutGuid);
        }

        xcmdline::parser::handle m_hPath;
    };

    //================================================================================================
    // ListLegalReferenceLibraries - discovery command for the resource-to-resource reference rule
    // ("Resources can have a dependency to other resources... as long as the other resources are part
    // of the dependency chain... it is a rule that must be observed and forced compliance" - direct
    // user requirement). Lists every library a resource OWNED BY -Library is legally allowed to
    // reference a resource from - itself, plus every library reachable by walking its own
    // m_ParentLibraries edges (xresource_editor::library_mgr::IsLibraryLegalReferenceTarget's own rule, exposed here
    // so an AI/script can check "am I allowed to point at this" without needing the ImGui picker at
    // all - same "never need to read a raw file / open a dialog by hand" reasoning every other
    // discovery command in this system was built for).
    // Usage: ListLegalReferenceLibraries -Library hexguid
    //================================================================================================
    struct list_legal_reference_libraries_query_cmd : xundo::query_command_base
    {
        list_legal_reference_libraries_query_cmd(xundo::system& System, void* pDataBase) noexcept : xundo::query_command_base(System, "ListLegalReferenceLibraries", pDataBase) { RegisterArguments(); }
        const char* getCommandHelp() const noexcept override
        {
            return "Lists every library a resource owned by -Library may legally reference a resource from (itself + its own transitive dependency chain). Usage: ListLegalReferenceLibraries -Library hexguid";
        }
        void RegisterArguments() noexcept override
        {
            m_hLibrary = m_Parser.addOption("Library", "Library instance guid, 16 hex digits", true, 1);
        }

        std::string Query() noexcept override
        {
            auto LibraryArg = m_Parser.getOptionArgAs<std::string>(m_hLibrary, 0);
            if (std::holds_alternative<xerr>(LibraryArg)) return "ListLegalReferenceLibraries: bad arguments";

            const auto LibraryGuid = xresource_editor::commands::ParseLibraryGuid(std::get<std::string>(LibraryArg));

            std::vector<xresource_editor::library::guid> Legal;
            xresource_editor::g_LibMgr.CollectTransitiveLibraryParents(std::vector<xresource_editor::library::guid>{ LibraryGuid }, xresource_editor::library::guid{}, Legal);

            std::string Out;
            for (auto& G : Legal)
            {
                std::wstring Path;
                xresource_editor::g_LibMgr.m_mLibraryDB.FindAsReadOnly(G, [&](const std::unique_ptr<xresource_editor::library_db>& DB) { Path = DB->m_Library.m_Path; });
                Out += std::format("{}  {}\n", xresource_editor::commands::FormatLibraryGuid(G), xstrtool::To(Path));
            }
            return Out;
        }

        xcmdline::parser::handle m_hLibrary;
    };

    //================================================================================================
    // ListLibraries - every library the project has open: its guid (what -Library takes), its path, whether it is the project's own root,
    // and the libraries it depends on.
    // Usage: ListLibraries
    //================================================================================================
    struct list_libraries_query_cmd : xundo::query_command_base
    {
        list_libraries_query_cmd(xundo::system& System, void* pDataBase) noexcept : xundo::query_command_base(System, "ListLibraries", pDataBase) {}
        const char* getCommandHelp() const noexcept override { return "Lists every open library: guid, path, whether it is the project root, and its dependencies. Usage: ListLibraries"; }
        void RegisterArguments() noexcept override {}

        std::string Query() noexcept override
        {
            std::string Out;
            for (auto& Lib : xresource_editor::g_LibMgr.m_mLibraryDB)
            {
                const auto& L = Lib.second->m_Library;
                Out += std::format("{}  {}{}", xresource_editor::commands::FormatLibraryGuid(Lib.first), xstrtool::To(L.m_Path), L.m_bRootProject ? "  [project root]" : "");
                if (!L.m_ParentLibraries.empty())
                {
                    Out += "  depends on:";
                    for (auto& P : L.m_ParentLibraries) Out += " " + xresource_editor::commands::FormatLibraryGuid(P.m_GUID);
                }
                Out += '\n';
            }
            return Out.empty() ? "(no open libraries)" : Out;
        }
    };
}

#endif // XRESOURCE_EDITOR_COMMANDS_LIBRARY_DEPENDENCY_H
