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

The `E10_` / `e10::` names are historical (the code grew out of xGPU's example E10); the manager and the views are the
resource pipeline's editor and are meant to be reused by every editor.

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
