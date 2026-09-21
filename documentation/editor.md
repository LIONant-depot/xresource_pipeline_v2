# source/editor - the resource pipeline's editor side

Everything an editor needs to browse and manage a project's resources. It compiles into the editor application (it is
not part of the resource compilers); nothing in `source/` (the pipeline itself) includes it.

| File | What it is | Needs UI? |
|---|---|---|
| `E10_AssetMgr.h` | `library_mgr` (`e10::g_LibMgr`): the open project, its libraries, descriptors, asset tree, trash, file operations, compile jobs | no |
| `E10_PluginMgr.h` | resource-plugin discovery, compiler processes, logs | no |
| `E10_PluginIconAtlas.h` | per-plugin icons: CPU atlas building (GPU upload is done by the browser) | no (CPU part) |
| `E10_SourceControlCache.h` | one shared cache of file source-control status and locks, read by every view | no |
| `E10_Resources.h` | bridge between `xresource_mgr` and xGPU (`resource_mgr_user_data`) | xGPU |
| `E10_AssetBrowser.h` | the browser window and its extension hooks (`m_OnRenameAsset`, `m_OnGetAssetStatusBadge`, ...) | ImGui |
| `E10_asset_browser_*_tab.h` | its tabs: Resources (virtual tree), Assets (files), Compilation, Project Settings (plugins), Search | ImGui |
| `E10_AssetOleDrag.h` | dragging assets out to Windows Explorer | ImGui / Win32 |
| `E10_CommandGuids.h` | how the resource commands write and read library and asset guids | no |
| `E10_Commands_Assets.h`, `E10_Commands_AssetFiles.h` | undoable commands: create / rename / move / delete / restore assets, and the same for raw files under a library | no |
| `E10_Commands_Compilation.h` | commands that drive the compile queue (`RecompileAll`, `CompileStart`, `CompileStatus`, ...) | no |
| `E10_SourceControlStatus.h` | background scans that fill the source control cache, and the per-library workspace sessions | no |
| `E10_Commands_SourceControl.h` | commit / pull / push / lock / unlock / revert as commands | no |
| `E10_Panel_SourceControl.h` | the Source Control panel (depots, changelists, file rows) | ImGui |
| `E10_AssetBrowserCallbacks.h` | `RegisterAssetBrowserCallbacks` and `RegisterSourceControlCallbacks`: wire the browser's `m_On...` hooks to an undo system | ImGui |

The `E10_` / `e10::` names are historical (the code grew out of xGPU's example E10); the manager and the views are the
resource pipeline's editor and are meant to be reused by every editor.

The commands derive from `xundo::command_base` / `query_command_base` and need no editor state: the `void*` database an
editor passes when it builds its command set is simply not used by them. The source control code lives here rather than in
`xsource_control` because it needs `library_mgr` and the browser, and this depot already depends on `xsource_control`
(`E10_SourceControlCache.h`); `xsource_control` stays the headless provider.

## Using it

An editor includes the headers it needs (paths are relative to the xGPU root, where the editor is built):

```cpp
#include "dependencies/xresource_pipeline_v2/source/editor/E10_AssetMgr.h"      // headless
#include "dependencies/xresource_pipeline_v2/source/editor/E10_AssetBrowser.h"  // UI
```

Editors customise the browser only through its `m_On...` hooks (all optional); an editor that leaves them unset gets
the plain browser. E29 wires them to its command/undo system so every browser action is also available to the AI/CLI.

## Known coupling to clean up

- The headers still include a few xGPU pieces (`source/xGPU.h`, `source/Tools/xgpu_imgui_breach.h`,
  `xgpu_xcore_bitmap_helpers.h`) for textures and ImGui helpers.
- `e10::g_LibMgr` is a process-wide global; the plan is to provide it as a service of the editor host instead.
