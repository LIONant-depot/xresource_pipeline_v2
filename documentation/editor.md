# source/editor - the resource pipeline's editor side

Everything an editor needs to browse and manage a project's resources. It compiles into the editor application (it is
not part of the resource compilers); nothing in `source/` (the pipeline itself) includes it.

| File | What it is | Needs UI? |
|---|---|---|
| `xresource_editor_asset_mgr.h` | `library_mgr` (`xresource_editor::g_LibMgr`): the open project, its libraries, descriptors, asset tree, trash, file operations, compile jobs | no |
| `xresource_editor_plugin_mgr.h` | resource-plugin discovery, compiler processes, logs | no |
| `xresource_editor_plugin_icon_atlas.h` | per-plugin icons: CPU atlas building (GPU upload is done by the browser) | no (CPU part) |
| `xresource_editor_source_control_cache.h` | one shared cache of file source-control status and locks, read by every view | no |
| `xresource_editor_resources.h` | bridge between `xresource_mgr` and xGPU (`resource_mgr_user_data`) | xGPU |
| `xresource_editor_asset_browser.h` | the browser window and its extension hooks (`m_OnRenameAsset`, `m_OnGetAssetStatusBadge`, ...) | ImGui |
| `xresource_editor_asset_browser_*_tab.h` | its tabs: Resources (virtual tree), Assets (files), Compilation, Project Settings (plugins), Search | ImGui |
| `xresource_editor_asset_ole_drag.h` | dragging assets out to Windows Explorer | ImGui / Win32 |
| `xresource_editor_command_guids.h` | how the resource commands write and read library and asset guids | no |
| `xresource_editor_commands_assets.h`, `xresource_editor_commands_asset_files.h` | undoable commands: create / rename / move / delete / restore assets, and the same for raw files under a library | no |
| `xresource_editor_commands_compilation.h` | commands that drive the compile queue (`RecompileAll`, `CompileStart`, `CompileStatus`, ...) | no |
| `xresource_editor_source_control_status.h` | background scans that fill the source control cache, and the per-library workspace sessions | no |
| `xresource_editor_commands_source_control.h` | commit / pull / push / lock / unlock / revert as commands | no |
| `xresource_editor_panel_source_control.h` | the Source Control panel (depots, changelists, file rows) | ImGui |
| `xresource_editor_asset_browser_callbacks.h` | `RegisterAssetBrowserCallbacks` and `RegisterSourceControlCallbacks`: wire the browser's `m_On...` hooks to an undo system | ImGui |

The code grew out of xGPU's asset-browser example (its old name was "E10", hence the former `E10_*.h` files and `e10::` namespace); it is now `xresource_editor_*.h` / `xresource_editor::`. The manager and the views are the
resource pipeline's editor and are meant to be reused by every editor.

The commands derive from `xundo::command_base` / `query_command_base` and need no editor state: the `void*` database an
editor passes when it builds its command set is simply not used by them. The source control code lives here rather than in
`xsource_control` because it needs `library_mgr` and the browser, and this depot already depends on `xsource_control`
(`xresource_editor_source_control_cache.h`); `xsource_control` stays the headless provider.

## Using it

An editor includes the headers it needs (paths are relative to the xGPU root, where the editor is built):

```cpp
#include "dependencies/xresource_pipeline_v2/source/editor/xresource_editor_asset_mgr.h"      // headless
#include "dependencies/xresource_pipeline_v2/source/editor/xresource_editor_asset_browser.h"  // UI
```

Editors customise the browser only through its `m_On...` hooks (all optional); an editor that leaves them unset gets
the plain browser. E29 wires them to its command/undo system so every browser action is also available to the AI/CLI.

## Known coupling to clean up

- The headers still include a few xGPU pieces (`source/xGPU.h`, `source/Tools/xgpu_imgui_breach.h`,
  `xgpu_xcore_bitmap_helpers.h`) for textures and ImGui helpers.
- `xresource_editor::g_LibMgr` is a process-wide global; the plan is to provide it as a service of the editor host instead.
