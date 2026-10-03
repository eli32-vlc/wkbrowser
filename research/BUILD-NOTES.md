# Progress log

## Part A: WebKit source build — BLOCKED, with the blocker identified

The build now reaches **74% (6697 of 8993 targets)** and fails on a Swift/C++
interop module error. This is a genuine toolchain requirement, not a script bug.

### The failure

```
Source/WebKit/Shared/RunJavaScriptResult.swift:27:8:
  error: no such module 'JavaScriptCore_Private.Cxx.DOMJITCallDOMGetterSnippet'
```

That file guards the interop imports with:

```swift
#if compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN
import JavaScriptCore_Private.Cxx.DOMJITCallDOMGetterSnippet
...
```

The GitHub runner's Swift satisfies `compiler(>=6.4)`, but the
`JavaScriptCore_Private.Cxx.*` modules only exist in **WebKit's own custom Swift
toolchain**, not in a stock Swift. The `!SWIFT_WEBKIT_TOOLCHAIN` half exists
precisely to skip them when building with WebKit's toolchain.

### Why this cannot be fixed with a flag

`SWIFT_WEBKIT_TOOLCHAIN` is a Swift **compilation condition**, not a C/C++
preprocessor macro, so `-DCMAKE_C_FLAGS=-DSWIFT_WEBKIT_TOOLCHAIN` cannot reach
the Swift compiler. WebKit supplies it via `build-webkit --swift-conditions`,
and there is no `OptionsCommon.cmake` / `OptionsGTK.cmake` knob for it. An
earlier patch that appeared to do this was reverted rather than left in as
decoration.

### The documented answer

WebKit's own build documentation states that for the Linux ports:

> Using the webkit-container-sdk is the recommended way to set up a
> development environment for the GTK or WPE ports, as it takes care of all
> the project dependencies and provides a configuration that closely matches
> the CI/CD testing setup.

That SDK image carries the custom Swift toolchain. Plain `ubuntu-latest`
cannot satisfy this. The container is not on Docker Hub under `webkit/*`, so
the exact image reference needs to be obtained from WebKit's CI config
(`Tools/CISupport`) or asked on their list before the next attempt.

Note the guard was added 2026-09-01, two weeks before the pinned SHA, so
pinning to a pre-September commit is also a viable route.

### What was fixed along the way (all real)

These were genuine architectural errors, each found by reading actual output:

- `build-webkit` **compiles during configure** — the dependency retry loop was
  repeatedly killing a legitimate multi-hour build and reporting
  "did not converge". Configure now calls `cmake -S/-B` directly.
- `build-webkit --generate-project-only` exits 0 in five seconds **without
  configuring**, producing a phantom `CONFIGURE_OK`. Removed.
- `gcc` is the runner default and rejects `-Wpass-failed`; WebKit needs clang.
- A space-containing `-D` value was destroyed by shell word-splitting, losing
  the clang warning flags three separate times. Now a real bash argv array.
- `cd src/WebKit` plus `-S src/WebKit` resolved to `src/WebKit/src/WebKit`.
- `rc=$?` after `if cmd; then ...; fi` captured the compound's status, not
  the command's, silently discarding timeout codes.
- A stray `cd` and a duplicate `esac` caused hard failures.
- The `apt-cache` fallback fuzzy-matched `libavahi-glib-dev` on an empty parse
  and burned all 8 attempts.

### Verified state of the workflow

Steps 1-9 pass. Configure succeeds in ~5 minutes, and the strip-flag assertion
(`USE_GSTREAMER` not ON in `CMakeCache.txt`) is **green**. The build then fails
at 74% on Swift.

**The RSS delta versus the 414 MB baseline remains unknown and must not be
quoted**, because no stripped library has ever been produced.

## Part B: embedder-level features — COMPLETE and CI-green

All verified by `src/test_p2.sh` and `src/test_t7.sh`, and gated by the
`test-embedder` workflow which runs in about two minutes:

| Item | Evidence |
|---|---|
| t7 file picker API | 4/4 assertions, upload bytes verbatim in multipart body |
| P2-1 ephemeral context | default `ephemeral`, `--persist` opts out |
| P2-2 asset blocking | `blocked=4 allowed=3`; control `blocked=0 allowed=7` |
| P2-3 domain blocklist | 53 entries parsed, GA request cancelled |
| P2-5 fingerprint probe | 22 vectors, 18 asserted, diff exits non-zero |

CI run 37086327924: all 7 steps success.

## Next step for Part A

Do **not** re-run the workflow as-is; it will fail at the same place. Pick one:

1. Switch the job to WebKit's `webkit-container-sdk` container image, which
   ships the required Swift toolchain.
2. Pin WebKit to a commit before 2026-09-01, before the interop guard landed.
3. Confirm with WebKit's CI config (`Tools/CISupport`) which Swift toolchain
   the GTK Linux bots use and reproduce that setup.

## Known limits that remain regardless

- Peak RSS ~414 MB unstripped; reducing it needs the blocked build.
- UA is still the stock Safari-60 value (P2-4 open).
- WebGL reports `Apple GPU | Apple Inc.` under Mesa, which no real Linux machine
  would report. Needs an engine patch.
- Asset blocking classifies by URI extension, because WebKitGTK 2.50.6's
  `WebKitURIRequest` exposes no `get_resource_type()`.
- `Notification.permission` reads `denied` on `data:` URLs but `default` on
  `http:`. That inconsistency is itself a signal; the probe asserts it.