# Changelog

## 1.1.0

UI rebuild for legibility and safety.

- Master–detail layout: a target rail, one target's keys at a time, and an
  inspector for the selected key. Replaces the single dense page that showed
  every target, group and key at once.
- Larger type and two-line rows so a key and its value no longer compete for
  width, with a `Compact` toggle for density when wanted.
- Inspector panel: status explanation, facts, emitted C++, resolution chain and
  per-key actions.
- Search now steps through matches (`Enter` / `Shift+Enter`) with a live
  `n/total` counter; filter chips hide themselves when a status is empty.
- Staged edits: typing never writes. A changed value is highlighted and applied
  only on `Enter` or an explicit Apply, with Discard to restore. Navigating away
  keeps staged edits rather than committing or dropping them.
- Undo/redo history maintained by the extension (a webview swallows `Ctrl+Z`),
  surfaced in the toolbar, on `Alt+Z`, and on the confirmation toast.
- Dirty indicator and Save button (`Ctrl+S`).
- Contextual banners when editing a base layer or when values cannot reach the
  compiler.
- Preset blocks can now be renamed (double-click a tab) and deleted, closing the
  create-only gap.
- New test suites: `render.js` drives the real front end on a DOM stub, and
  `ui-contract.js` statically checks the webview/host message, icon, class and
  command contracts.

## 1.0.0

Initial release.

- Custom editor for `*_preset.json` / `*_presets.json` with inline editing,
  inherited-value comparison, per-status filters and instant search.
- Native webview context menu covering value/metadata edits, cross-file
  overrides, group moves, workspace-wide rename, duplicate, delete and
  navigation.
- Cross-file resolution engine reproducing `generate_tuning.py`, including
  file-level priority, strict-greater override, registration-order tie-breaking,
  per-target visibility cut-off and `constexprs.*` flattening.
- CMake scanner that follows `add_subdirectory()` and replays user-defined
  functions at their call sites, so targets created inside helpers resolve with
  the correct registration index.
- Diagnostics for unregistered files, missing constexpr types, shadowed
  overrides, priority inversions, duplicate keys, orphan targets and redundant
  overrides.
- Sidebar tree over targets, presets, groups and keys with full layer traces.
- Commands for key search, key resolution, preset comparison and a workspace
  report.
- Style-preserving JSON writer: identical indent, empty-container spelling,
  trailing newline and key order.
