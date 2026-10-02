# WPE Automation Browser — Phase 0 Research

Date: 2026-10-02. Host: Debian 12 (bookworm), x86_64.

## RESULT SO FAR (updated after live experiments)

1. **WPE 2.38.6 cannot run headless on this host.** Confirmed by experiment, not
   theory. WPEWebKit requires an embedder-supplied view backend (2.38 predates
   built-in `wpe-display-headless`). A custom null backend avoids the dlopen of
   `libWPEBackend-default.so`, but the crash is in the **web process**, which
   independently loads `libWPEBackend-fdo-1.0.so` and segfaults creating an EGL
   display with no display server. `EGL_PLATFORM=surfaceless`,
   `LIBGL_ALWAYS_SOFTWARE=1`, `WEBKIT_DISABLE_DMABUF_RENDERER=1` all still
   SIGSEGV. WPE needs a real GPU stack; unusable headless here.
2. **WebKitGTK 2.50.6 works headless under Xvfb.** Compiled and ran, loaded a
   page, reported the correct title, zero JS output issues. Xvfb is 3 MB. This
   is the working baseline.
3. Disk so far: project dir **under 100 KB**; system deps ~1.5 GB via apt.
4. Blocked items needing engine patches (WebKit source build, not viable on
   3.4 GB RAM): `navigator.webdriver`, WebGL renderer, build-time media strip.

## Hard constraint

**Total project disk footprint must never exceed 10 GB.** Budget lives at
`/root/wk-project` (source, build, dist, logs, ccache). Apt-installed system
packages are treated as OS baseline, not project — but their size is tracked
in `system-deps.txt` for transparency.

## Host capability audit (measured)

| Resource | Value | Impact |
|---|---|---|
| `/` free | 23 GB | Fits 10 GB budget with ~13 GB headroom |
| `/tmp` | **tmpfs, 337 MB** | **Never build in /tmp.** Uses RAM, and RAM is scarce. |
| CPU | 4 cores | WebKit Release build ≈ 6–12 h |
| RAM | 3.4 GB total, 2.5 GB already used, ~855 MB available | **This is the binding constraint.** |
| Swap | 3.1 GB (1.5 GB already used) | Linking WebCore will swap hard |

Build toolchain present: `git`, `python3`.
Missing (needed for a source build): `cmake`, `ninja-build`, `gperf`, `bison`,
`flex`, `ccache`.

## Available prebuilt engines (Debian bookworm)

| Package | Version | Download | Notes |
|---|---|---|---|
| `libwebkit2gtk-4.1-0` | 2.50.6 | (installed) | GTK4 port, pulls GTK3 + GStreamer |
| `libjavascriptcoregtk-4.1-0` | 2.50.6 | 7.2 MB | JSC only |
| `libwpewebkit-1.1-0` | **2.38.6** | 23 MB | **WPE port — no GTK.** Available. |
| `libwpewebkit-1.1-dev` | 2.38.6 | 68 KB | Embedder headers |
| `libwpe-1.0-1`, `libwpebackend-fdo-1.0-1` | 1.14 | small | FDO backend |

Both ports are packaged. WebKitGTK is newer (2.50.6); WPE in bookworm is older
(2.38.6) because bookworm froze before newer WPE landed.

## Disk cost model

- WebKit git objects, full history: **~13.1 GB** (`size` field from GitHub API,
  13136351 KB). A non-shallow clone **blows the entire budget by itself.**
- Release (no debug info) WPE build dir: **~4–6 GB**
- Source checkout at depth 1: **~1.5–2 GB**
- ccache: cap at 2 GB with `CCACHE_MAXSIZE=2G`

Naive approach: 13 GB (clone) + 6 GB (build) = **19 GB — over budget.**
With `--depth 1` + Release + no debug info + ccache cap: **~8 GB — fits, barely.**

## Verdict on source-building WebKit here

**Not currently viable on this host, for RAM reasons — independent of disk.**

Linking `libwebkit.so` on a 4-core / 3.4 GB box needs roughly 8–16 GB of
address space even with `lld` + `--no-keep-memory`. Available RAM + swap is
~4.4 GB total. It will OOM or thrash for many hours per link step, and there
are several link steps.

Two viable paths (Path A as originally planned has since been disproven —
see "Path A result" below):

**Path A0 — superseded: binary WPE. DEAD.** WPE 2.38.6 cannot run headless on
this host; the web process segfaults in `libWPEBackend-fdo` creating an EGL
display. Do not retry without a real GPU/display stack.

**Path A — binary WebKitGTK, no source build (ACTIVE).**
Link against Debian's `libwebkit2gtk-4.1-0` (2.50.6) and run under Xvfb.
Achieve every Phase-1 goal that does not require patching WebKit internals:
- file-picker API (`run-file-chooser` signal) ✅ pure embedder
- geolocation / screen / battery fake injection ✅ via `UserContentManager` JS
  injection before page scripts run + WebKitSettings
- device enumeration override ✅ JS injection
- headless ✅ via Xvfb
- disk: **< 500 MB total**
Cost: cannot remove `navigator.webdriver` (that needs `NavigatorID.cpp`) and
cannot strip media/GPU at build time.

**Path B — source build WebKit (later, needs more RAM or a bigger box).**
`--depth 1`, Release, `-Og`, no debug info, ccache 2 GB, out-of-tree build into
`build/`, `WEBKIT_OUTPUTDIR`. Expect ~8 GB and 6–12 h. Must be re-validated
against free disk before starting. This is the only path that unblocks
`navigator.webdriver`, WebGL renderer spoofing, and build-time media strip.

## Path A result: WPE dead, WebKitGTK viable

WPE's "light" advantage is moot if it cannot run headless. Decision: **WebKitGTK
2.50.6 + Xvfb**. Heavier at runtime than WPE would be (~100 MB vs ~60 MB RSS)
but it is the only WebKit port that works display-less here, and every
embedder-level feature (file chooser API, user content injection, settings,
resource loading) is identical in shape to WPE's.

Note this reverses the earlier recommendation. Headless capability outranks
binary size.

WPE now ships headless natively (no X11, no Wayland, no compositor needed):

```sh
WPE_DISPLAY=wpe-display-headless ./launcher https://example.com
```

Available in 2.52+ via WPEPlatform (`-DENABLE_WPE_PLATFORM=ON`, default from
2.54). **Debian's 2.38.6 predates this.** For 2.38.6 the headless route is
either WPEBackend-FDO in offscreen/SHM mode, or `--use-gl=angle`/software
rendering. This must be proven experimentally before building anything — it is
task t4 and it is the single biggest technical risk in Path A.

Mesa software rendering is already present (`swrast`/llvmpipe DRI drivers,
EGL 1.6, Vulkan ICDs present), so GPU-free operation is plausible.

## Critical unknowns to resolve in t4

1. ~~Does `libwpewebkit-1.1` 2.38.6 run with **no** `DISPLAY`?~~ **RESOLVED: no.**
2. ~~Can frames be discarded without a compositor?~~ **Unreachable — WPE dead.**
3. Does software GL work well enough under Xvfb? (WebKitGTK probe passed; needs
   proper RSS + heavy-page measurement.)
4. Does `run-file-chooser` fire without a UI-process dialog? (Expected yes.)

If (3) or (4) fail, options are: build a custom `WebKitWebViewBackend`-free path
(WebKitGTK has no backend interface, so this cannot be repeated), or move to a
source build with a patched compositor-free path.

## Anti-detection reality check

Blocked on Path A because it needs engine patches:
- `navigator.webdriver` — set by the automation controller in `NavigatorID.cpp`
- WebGL `UNMASKED_RENDERER_WEBGL` — leaks real ANGLE/Mesa stack
- UA + Client Hints consistency
- font set, timezone, `hardwareConcurrency`

Achievable on Path A without patching:
- consistent UA/Client-Hints via UA override + request header rewriting
- `hardwareConcurrency`/`deviceMemory` via JS injection
- font set control via fontconfig
- geolocation/screen/battery/permissions via injection

Expect ~60% of the anti-detection surface on Path A, 100% only after Path B.

## PHASE 1 RESULTS (verified by running code)

### t5 — fake value API: WORKING
All values verified reaching the page via DOCUMENT-END injection:

```
$ xvfb-run -a ./dist/wkbrowser --url "data:text/html,<h1>probe</h1>" --probe \
    --screen 1920x1080 --cores 8 --geo 51.5074,-0.1278 --tz "America/New_York"
{"ua":"...Safari/605.1.15","webdriver":false,"cores":8,"platform":"Linux x86_64",
 "screen":[1920,1080,24],"dpr":1,"tz":"America/New_York","devices":"n/a"}
```

Confirmed overridden: screen width/height, devicePixelRatio, colorDepth,
hardwareConcurrency, geolocation getCurrentPosition/watchPosition, timezone
(Intl resolvedOptions + Date.getTimezoneOffset), navigator.userAgent,
navigator.platform, and a synthetic mediaDevices.enumerateDevices list.

`navigator.webdriver` is already `false` — see t8 below.

### t7 — file picker API: WORKING
`run-file-chooser` fires, no GTK dialog is constructed, the registry path is
returned via `webkit_file_chooser_request_select_files()`. Full acceptance test
in `src/test_t7.sh`:

```
[t7] run-file-chooser fired (NO dialog)
[wkb] file chooser satisfied with .../payload.txt
RESULT: {"count":1,"name":"payload.txt","size":29}
multipart body: 316 bytes
payload present verbatim: True
*** T7 PASS ***
```

Upload bytes verified verbatim inside the multipart body against a local HTTP
server.

### t8 — automation markers: PARTIAL
- `navigator.webdriver` is **already false** in this build (the automation
  controller is a Playwright/WebDriver addition, not present in stock
  WebKitGTK 2.50.6). No patch needed.
- The UA is an **anomalous Safari-60-era string** — a strong fingerprint on its
  own. Needs a coherent modern profile (e.g. Chrome 131 on Windows 11, or
  Firefox 133 on Linux) with matching Client Hints. Not yet done.
- WebGL `UNMASKED_RENDERER_WEBGL` still leaks the real Mesa/ANGLE stack.
  Requires engine patch.

### Resource profile (measured)
- Peak RSS for a full page load: **~414 MB** across UI process + WebKitWebProcess
  + WebKitNetworkProcess + Xvfb. Higher than the "super light" goal; this is
  the unstripped build with GStreamer media and GPU paths present.
- Project disk: **under 200 KB**. Apt system deps ~1.5 GB (outside the 10 GB
  project budget). Far under the 10 GB cap.

## Key source-level notes learned

- WebKitGTK 4.1 renamed `run_javascript` → `evaluate_javascript`, and
  `_finish` returns `JSCValue*` directly (no `WebKitJavascriptResult`). Getting
  this wrong produces `JSC_IS_CONTEXT` assertion failures.
- Enum names: `WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES` (not `...INJECTED_FRAMES_ALL`)
  and `WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END`.
- `webkit_web_view_new_with_context()` returns `GtkWidget*`, which is
  `WebKitWebView*` by the classic cast in the GTK API. A strict compiler warns;
  the cast is correct.
- The `--file` registry is consulted in `run-file-chooser`; there is no way to
  pre-populate `input.files` from the embedder, so automation must trigger the
  picker (a synthetic click or `input.click()`). This is inherent, not a bug.

**Legal note.** Automating your own identity
impersonating other users to evade rate limits, or scraping against a site's
ToS carries real civil and (in the US) CFAA-adjacent exposure — HiQ v. LinkedIn
settled public-page scraping under the CFAA but lost on the ToS claim. This
project is scoped to the user's own profile and legitimate automation.
Automating your own identity is ordinary tooling. Defeating CAPTCHAs/MFA,
impersonating other users to evade rate limits, or scraping against a site's ToS
carries real civil and (in the US) CFAA-adjacent exposure — HiQ v. LinkedIn
settled public-page scraping under the CFAA but lost on the ToS claim. This
project is scoped to the user's own profile and legitimate automation.
