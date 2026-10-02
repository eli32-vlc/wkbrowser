# Phase 2 Backlog — scraping-first

Ordered by value-per-cost. Items 1–5 need **no WebKit source build**; they are
embedder-level and work on the current WebKitGTK 2.50.6 binary. Item 6 is the
only one that hard-requires the GitHub Actions source build.

Status legend: `[ ]` open · `[x]` done · `[~]` partial

---

## P2-1 `[ ]` Ephemeral context per navigation

**Why:** cookies, localStorage, IndexedDB and HTTP auth persist in a
`WebKitWebContext` for its whole lifetime. A scraper that reuses one context
leaks its own session between sites — both a correctness bug and a fingerprint.

**Where:** `src/wkbrowser.c`, context creation.

**Do:**
- Use `webkit_web_context_new_with_ephemeral_session()` for the default
  single-shot scrape path.
- Add `--persist` flag that opts back into
  `webkit_web_context_new_with_session_data()` for flows that genuinely need
  a stable session (multi-step login).
- Store the context on the profile so it can be swapped per navigation.

**Accept:** load two pages on different origins in one process; a
`document.cookie` set on the first is not visible on the second.

**Cost:** ~15 lines. No build.

---

## P2-2 `[ ]` Block images, fonts and media

**Why:** the single largest scraping speed and bandwidth win. Also cuts
third-party origins that would otherwise observe the visit.

**Do:**
- Add `--block-assets` flag. When set, cancel subresources by type in a
  `notify::resource-load-started` handler on the web view: skip
  `RESOURCE_TYPE_IMAGE`, `RESOURCE_TYPE_FONT`, `RESOURCE_TYPE_MEDIA`.
- Keep `RESOURCE_TYPE_DOCUMENT`, `SCRIPT`, `STYLESHEET`, `XHR`, `FETCH`,
  `WEBSOCKET` — those carry the actual content.
- Add `--block-stylesheets` separately, because many sites hide content behind
  JS-injected CSS but a few break entirely without it. Default off.

**Accept:** with `--block-assets`, a page with 40 images loads in visibly less
wall time and `performance.getEntriesByType('resource')` contains no `img`
entries, while the DOM still parses identically.

**Cost:** ~30 lines. No build.

---

## P2-3 `[ ]` Block analytics and beacon noise

**Why:** third-party scripts are the highest-fidelity tracking vector and the
most common cause of a scraper being fingerprinted by request-pattern
analysis. Blocking them also cuts load time.

**Do:**
- Ship a JSON rule file `share/wkb-blocklist.json` in
  `WebKitUserContentFilter` format, covering the common analytics/beacon
  domains: Google Analytics, DoubleClick, Segment, Sentry, Hotjar, Mixpanel,
  Facebook pixel, Intercom, FullStory, Clarity.
- Expose `--blocklist FILE` to swap in a custom list; `--no-blocklist` to
  disable.
- Document that this is a coarse net, not a privacy tool — it reduces signal,
  it does not eliminate it.

**Accept:** with the default blocklist, a test page loading GA + Sentry shows
neither `google-analytics.com` nor `sentry.io` in the resource timing list.

**Cost:** ~40 lines plus a data file. No build.

---

## P2-4 `[~]` Coherent modern UA + Client Hints

**Status:** UA override via injection works today. Client Hints do **not**.

**Why it matters:** the stock WebKitGTK 2.50.6 UA is

```
Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/60.5 Safari/605.1.15
```

That is a Safari 60-era string. It is internally inconsistent with a modern
Linux Chromium build, and the mismatch between UA and the actual engine
(JavaScriptCore) is itself a signal. Any target that cross-checks UA against
`Sec-CH-UA` headers, engine hints, or feature support will flag it.

**Do:**
- Define named profiles in `src/wkb_profile.c`, e.g. a coherent
  `chrome131-linux` (UA string, `Sec-CH-UA`, `Sec-CH-UA-Platform`,
  `Sec-CH-UA-Mobile`, `navigator.userAgentData` high-entropy values) that all
  agree with each other.
- Add `--profile NAME` to select one; keep `--ua` as a raw escape hatch.
- Override the request `User-Agent` header too, not just the JS-visible
  property — a page reading `navigator.userAgent` but seeing a different
  header on its own request is a trivial detection.
- **Known gap:** WebKitGTK sends no `Sec-CH-UA` headers at all. Absent Client
  Hints are anomalous against a real Chrome. Fully fixing this needs a source
  patch, tracked in P2-6.

**Accept:** `--profile chrome131-linux` produces a probe where
`navigator.userAgent`, the outgoing `User-Agent` header, and any
`userAgentData` values all match one another, with no Safari tokens.

**Cost:** ~60 lines for the JS side; header override needs a
`webkit_uri_scheme`/`WebKitURIFilter` or source patch.

---

## P2-5 `[ ]` Automated detection probe with before/after diff

**Why:** every change in P2-1…P2-4 can accidentally *increase* detectability.
Without a regression test you are flying blind, and several of these changes
(removing Notification support, for example) are net-negative precisely
because that is not obvious.

**Do:**
- `src/probe_detect.js` — a single self-contained script that reports the
  fingerprint vector set as JSON: UA, `userAgentData` presence, `webdriver`,
  `hardwareConcurrency`, `deviceMemory`, platform, vendor, screen metrics,
  dpr, colorDepth, timezone, languages, `Notification.permission`,
  `navigator.plugins.length`, `pdfViewerEnabled`, WebGL
  `UNMASKED_RENDERER_WEBGL` + `VENDOR`, `navigator.permissions` behaviour,
  `chrome` object presence, error-stack format, `toString()` of builtins.
- `src/probe_diff.sh` — run the probe with a given flag set, before and
  after, and emit a unified diff plus a **PASS/FAIL** against a checked-in
  `tests/expected_fingerprint.json` baseline.
- Wire it into CI so a change that breaks the fingerprint fails the build.

**Accept:** `probe_diff.sh` exits non-zero when any field deviates from the
baseline, and the diff is reviewable in the PR.

**Cost:** ~120 lines. Highest leverage item in the backlog — do it before P2-4
so you can measure P2-4.

---

## P2-6 `[ ]` Build-time media/GPU strip (needs Actions source build)

**Why:** the only item that satisfies "removed at build time, not stubbed at
runtime". Everything above can be done with injection; this genuinely deletes
code.

**Baseline to beat:** 414 MB peak RSS, unstripped.

**CMake flags** (verified against `Source/cmake/OptionsGTK.cmake` upstream):

```
-DENABLE_WEB_RTC=OFF
-DENABLE_WEBXR=OFF -DENABLE_WEBXR_AR=OFF -DENABLE_WEBXR_HANDS=OFF
-DENABLE_WEBXR_HIT_TEST=OFF -DENABLE_WEBXR_LAYERS=OFF
-DENABLE_GAMEPAD=OFF
-DENABLE_MEDIA_STREAM=OFF
-DENABLE_WEB_CODECS=OFF
-DENABLE_SPEECH_SYNTHESIS=OFF
-DENABLE_MHTML=OFF -DENABLE_PDFJS=OFF -DENABLE_XSLT=OFF
-DENABLE_JOURNALD_LOG=OFF
-DENABLE_WEBDRIVER=OFF
-DENABLE_DEVELOPER_MODE=OFF
-DENABLE_EXPERIMENTAL_FEATURES=OFF
-DUSE_GSTREAMER=OFF
```

**The `USE_GSTREAMER` trap** (upstream PR #61274): `USE_GSTREAMER=OFF` is
unconditionally reset to `TRUE` if any of `ENABLE_VIDEO`, `ENABLE_WEB_AUDIO`,
`ENABLE_WEB_CODECS` is set. The workflow must therefore also pass
`-DENABLE_VIDEO=OFF -DENABLE_WEB_AUDIO=OFF` **or** patch
`Source/cmake/GStreamerChecks.cmake`. Verify after building with:

```sh
readelf -d libwebkitgtk-*.so | grep -i gst   # must be empty
```

**Do:**
- Shallow clone `--depth 1` (full history is ~13 GB and blows the cap alone).
- Release, `-Og`, no debug info, ccache capped at 2 GB.
- Artifact: the built `.so` + a `Deb`/tarball, uploaded so the binary is
  reusable without rebuilding.

**Accept:** `readelf -d` shows no `libgst*` dependency; `enumerateDevices()`
returns empty and `getUserMedia` rejects because the code is not compiled;
peak RSS measured and compared to 414 MB.

**Cost:** ~2–4 h of Actions minutes per clean build. Build rarely.

---

## Explicitly NOT doing

- **Removing Notification support.** With no display it is already inert.
  Worse, `Notification.permission === "default"` is what a real browser looks
  like; making it throw or hard-deny is a *larger* fingerprint than leaving it
  native. If needed, fake it as granted with a no-op `Notification` object.
- **Removing spellcheck, autocorrect, drag-support.** Negligible binary
  savings, nonzero breakage risk, no scraping value.
- **Chasing "undetectable".** Fingerprint evasion against a specific site is
  a treadmill, and sites change weekly. Build P2-5 so regressions are visible,
  and treat passing a given detector as an outcome, not a guarantee.

## Legal boundary

Scope is the user's own identity and legitimate automation. Do not add
CAPTCHA/MFA defeat, multi-identity rotation to evade rate limits, or
scraping that ignores a site's terms of service. HiQ v. LinkedIn settled the
CFAA question for public-page scraping but the ToS claim still failed, so ToS
is live civil risk even where the CFAA is not.
