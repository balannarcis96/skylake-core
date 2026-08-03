# Skylake Tuning Presets — VS Code extension

Manage the `SkylakeTuning` preset JSON files (`cmake/presets/**`) as what they
actually are: layered configuration that CMake merges into generated C++
headers. Zero dependencies, zero build step.

The point of the extension is context. A preset file on its own cannot tell you
what a value overrides, whether your override survives the merge, or whether
CMake reads the file at all — those answers live across the preset tree and the
`CMakeLists.txt` files. This surfaces all of it inline.

## Features

**Custom editor** — opening any `*_preset.json` / `*_presets.json` renders a
structured view instead of raw JSON (`Reopen as Text` is always one click away).

Laid out as master–detail so nothing has to compete for space:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ● core/default_preset.json   cmake/presets/…   #5     Priority 0  Ver 1  ↶↷ 💾│
├──────────────────────────────────────────────────────────────────────────────┤
│  default │ dev 12 │ qa 8 │ prod 9 │ +                                        │
├───────────────┬──────────────────────────────────────────┬───────────────────┤
│ TARGETS       │ ⌕ search…            2/7  ↑ ↓ ✕          │ DETAILS         ✕ │
│ ▸ libthecore  ├──────────────────────────────────────────┤                   │
│      63       │ ◎ libthecore    libthecore   base layer  │ CHWID_SlotUseCount│
│ ▸ libevent  ● │   Namespace [      ]  Output [public ▾]  │ [Tune] How many … │
│       9       │                                          │                   │
│ ▸ diag_hub    │ ▾ constexprs.hwid                    5   │ ⓘ Overrides an    │
│       1       │  ┃ CHWID_SlotUseCount          u32  pub  │   inherited value │
│               │  ┃ [ 2u          ]   was · 5u            │                   │
│ + Add target  │  ┃ CHWID_ChangeTime            u32  pub  │ FACTS             │
│               │  ┃ [ (60u*60u*24u) ]  new key            │  Value    2u      │
│               │                                          │  Header   public  │
│               │ ▾ constexprs.libsql                  9   │                   │
└───────────────┴──────────────────────────────────────────┴───────────────────┘
   new · override · redundant · shadowed        / search  ↑↓ move  ⏎ apply
```

- **One target at a time.** The rail picks it; a per-target key count and a
  warning dot for anything needing attention.
- **Two-line rows** — key and tags on top, value and comparison underneath, so
  neither is truncated. `Compact` halves the height when you want density back.
- Every row carries its **inherited value** and a status:
  `introduced` · `override` · `redundant` · `shadowed`.
- **Inspector** for the selected key: status explanation, facts, the exact C++
  that gets emitted, the resolution chain, and the actions.
- Search with **match stepping** (`Enter` / `Shift+Enter`), a live `n/total`
  counter, and status filter chips that hide themselves when a status has no
  members.
- Greyed **inherited keys** the preset does not override, each with an
  `Override here` button.
- Contextual banners: editing the base layer, or values that never reach the
  compiler.
- Native right-click menu on any row: change value, edit type/description,
  toggle public/private, override in another preset, move group, rename
  (this file or workspace-wide), duplicate, delete, jump to the definition it
  overrides, resolve across presets, reveal in the raw JSON.

**Accident prevention** — the editor writes to your source tree, so nothing
happens by surprise:

| Guard | Behaviour |
| --- | --- |
| Staged edits | Typing never writes. A changed value is highlighted and applied only on `Enter` or an explicit **Apply**; **Discard** restores it |
| No silent loss | Clicking away keeps the staged edit exactly where it was instead of committing or discarding it |
| Undo / redo | Every write is reversible from the toolbar, `Alt+Z`, or the **Undo** button on the confirmation toast — a webview swallows `Ctrl+Z`, so the extension keeps its own snapshot history |
| Destructive ops | Deleting a key, group, target config or preset is modal-confirmed, with the entry count spelled out |
| Base-layer warning | Editing a `DEFAULT_PRESET_FILE` says so: the change hits dev, qa and prod at once |
| Shadowed warning | A banner and per-row marker when a value cannot reach the compiler |
| Dirty state | Unsaved changes show a dot next to the filename and light up **Save** (`Ctrl+S`) |

**Keyboard** — `/` or `Ctrl+F` search · `Enter`/`Shift+Enter` step matches ·
`↑`/`↓` move between rows · `Enter` open details · `Esc` cancel an edit or clear
search · `Alt+Z` undo · `Ctrl+S` save.

**Cross-file resolution** — a faithful port of `generate_tuning.py`, including
the rules that make the merge surprising:

- Priority lives on the *file*, not the entry.
- A later file wins only on a **strictly greater** priority; ties keep the
  incumbent, so registration order decides them.
- A target only sees files registered *before* its own
  `skl_add_tune_header_to_target()` call.
- Every `constexprs.<suffix>` bucket is flattened into one table, so the suffix
  is presentation only.
- The `Priority:` reported in a generated header is the maximum priority
  *scanned*, not the winning one.

**Diagnostics** in the Problems panel:

| Code | Severity | Meaning |
| --- | --- | --- |
| `constexpr-missing-type` | Error | New constexpr without `type` — `generate_tuning.py` exits `-1` |
| `unregistered-file` | Warning | No `skl_add_presets_file()` and not a `DEFAULT_PRESET_FILE`; CMake never reads it |
| `shadowed-override` | Warning | A higher-priority file wins; this value never reaches the compiler |
| `priority-inversion` | Warning | Registered after a higher-priority file for the same preset/target, so it can never override it |
| `duplicate-key` | Warning | Only the last occurrence survives `json.load` |
| `target-without-consumer` | Info | No `skl_add_tune_header_to_target()` uses this `target_name` |
| `redundant-override` | Hint | Restates the inherited value |

**Sidebar** — Targets → presets → groups → keys, each key showing its resolved
value and a full layer trace on hover. Plus a Files view with registration
status.

**Commands** (`Skylake Tuning:` prefix)

| Command | Purpose |
| --- | --- |
| Search Keys… | Fuzzy search every key in the workspace; flags keys that differ per preset |
| Resolve Key Across Presets… | Per-preset effective value, full layer trace, and the emitted C++ |
| Compare Two Presets… | Value-level diff between two presets for one target |
| Show Workspace Report | Registration order, dead files, orphan targets, per-target summary |
| Validate All Preset Files | Re-index and open Problems |
| Rescan Workspace | Force a re-index |

## Install

No build, no `npm install`. Copy or symlink the folder into your extensions
directory and reload the window:

```bash
# WSL / Linux remote
ln -s /home/dev/projects/skylake-core/tools/vscode_presets_plugin \
      ~/.vscode-server/extensions/skylake-tuning-presets-1.0.0

# Local Linux/macOS
ln -s /home/dev/projects/skylake-core/tools/vscode_presets_plugin \
      ~/.vscode/extensions/skylake-tuning-presets-1.0.0
```

Then `Developer: Reload Window`.

To hack on it, open this folder in VS Code and press `F5` for an Extension
Development Host.

To produce a `.vsix`, `npx @vscode/vsce package` — needed only for distribution.

## Tests

The resolver claims to reproduce `generate_tuning.py`, so the test suite checks
that claim against real repositories rather than fixtures:

```bash
node test/run.js                      # auto-discovers this repo + a sibling m2-server
node test/run.js /path/to/repo        # or point it somewhere explicit
```

- `verify.js` — parser/serializer round-trip fidelity, CMake registration order
  against `SKL_TUNE_PRESETS_FILES` in `build/CMakeCache.txt`, and every resolved
  value diffed against the generated `tune_*.h` headers. Distinguishes a
  resolver bug from a stale build artifact by content.
- `integration.js` — drives the index, view model, all mutations, diagnostics
  and the tree through a stubbed `vscode` module.
- `render.js` — mounts the real webview front end on a DOM stub and drives it:
  renders every model in the workspace, switches targets, searches, filters,
  selects rows, and asserts the staged-edit guarantees (typing writes nothing,
  Discard restores, `Enter` writes exactly once).
- `ui-contract.js` — static checks that every webview message has a host
  handler, every icon and CSS class referenced exists, and every declared
  command is registered.

Current status: **160 checks across 7 suites, all passing**. Against
`m2-server`, 145 resolved values match the generated headers exactly across all
9 targets, and all 30 preset files round-trip byte-for-byte.

## Editing model

Mutations rewrite the whole document and hand it to VS Code as one
`WorkspaceEdit`, so undo/redo, dirty state, save and git decorations all behave
normally. The writer reproduces the file's existing style — indent width, empty
container spelling (`{}` vs `{\n}`), trailing newline — so a one-value change
produces a one-line diff. Key order is preserved everywhere; renaming a key
leaves it in place rather than moving it to the end.

New entries use the shape the generator requires: overrides are written in the
shorthand `"KEY": "value"` form, while a key introduced for the first time gets
the full `{ "value", "type", "desc" }` object, because a new constexpr without
`type` aborts the generator.

## Known limits

- Conditionals are not evaluated. A `skl_add_presets_file()` inside an `if()`
  block is still reported and flagged *conditional*, rather than guessed at.
- User-defined functions are captured and replayed at each call site, which is
  what makes `libskl-core` and `game_auth` resolve correctly. Variables assigned
  inside a conditional *within* such a function (`make_game_target` picking
  `TARGET_NAME`) resolve to the last assignment seen; only the display label is
  affected, never `PRESET_TARGET_NAME`.
- `include()` is not followed.
- Values containing `@VAR@` or `${VAR}` are rewritten by `configure_file()`
  after the generator runs (it is called without `@ONLY`). These are shown with
  an `@` badge; the extension does not attempt to evaluate them.
