# wkbrowser — headless WebKit automation browser (Phase 1)

A WebKitGTK 2.50.6 embedder for headless web automation on Linux. Runs with no
visible display (Xvfb), replaces the native file picker with an API, and
injects a configurable synthetic-value profile.

## Status

| Task | Status | Evidence |
|---|---|---|
| t1 research + disk model | done | `research/FINDINGS.md` |
| t2 10 GB budget guard | done | `research/budget.sh` |
| t4 headless proof | done | WebKitGTK+Xvfb works; **WPE abandoned** |
| t5 fake value API | **done, verified** | probe output in FINDINGS.md |
| t7 file picker API | **done, verified** | `src/test_t7.sh` passes |
| P2-1 ephemeral context | **done, CI-green** | `src/test_p2.sh` |
| P2-2 asset blocking | **done, CI-green** | blocked=4 allowed=3 |
| P2-3 domain blocklist | **done, CI-green** | 53 entries parsed |
| P2-5 fingerprint probe | **done, CI-green** | 22 vectors, diff exits non-zero |
| P2-4 UA profiles | open | needs coherent Client Hints, source patch |
| P2-6 source build | **NOT done** | no green build, RSS delta unknown |

Full status and the defects CI surfaced: see `PROGRESS.md`.

## Build

```sh
./src/build.sh          # produces dist/wkbrowser
```

## Run

Xvfb supplies the display WebKitGTK requires:

```sh
xvfb-run -a -s '-screen 0 1280x800x24' ./dist/wkbrowser --url URL [options]
```

### Options

Run `./dist/wkbrowser --help` for the authoritative list.

```
--url URL          page to load (required)
--dump             print document.body.innerText after load
--probe            print the fingerprint vector set as JSON
--file PATH        register PATH as the file the picker returns
--screen WxH       fake screen dimensions
--geo LAT,LON      fake geolocation
--cores N          fake navigator.hardwareConcurrency
--memory GB        fake navigator.deviceMemory
--ua STRING        fake navigator.userAgent
--tz STRING        fake Intl timezone

network:
--block-assets     cancel images, fonts and media
--block-styles     also cancel stylesheets
--blocklist FILE   blocklist file (default share/wkb-blocklist.txt)
--no-blocklist     disable blocking
--block DOMAIN     block one domain substring (repeatable)
--persist          keep cookies/storage instead of an ephemeral session
```

## Example: verified fake-value run

```sh
$ xvfb-run -a ./dist/wkbrowser --url "data:text/html,<h1>probe</h1>" --probe \
    --screen 1920x1080 --cores 8 --geo 51.5074,-0.1278 --tz "America/New_York"
{"cores":8,"platform":"Linux x86_64","screen":[1920,1080,24],"dpr":1,
 "tz":"America/New_York","devices":"n/a","...":"..."}
```

## Tests

```sh
./src/test_t7.sh       # file chooser + byte-exact upload verification
./src/test_p2.sh       # ephemeral context, asset blocking, blocklist
./src/probe_check.sh   # fingerprint regression against the baseline
```

All three run in CI on every push via `.github/workflows/test-embedder.yml`,
which completes in about two minutes.

Refresh the fingerprint baseline deliberately:

```sh
./src/probe_check.sh --update
```

## Architecture

- `src/wkb.h` — profile struct + public API
- `src/wkb_profile.c` — synthetic-value injection (DOCUMENT-END user scripts)
- `src/wkb_files.c` — file registry / `run-file-chooser` handler
- `src/wkb_net.c` — ephemeral context, asset blocking, domain blocklist
- `src/wkb_probe.c` — the fingerprint vector set
- `src/wkbrowser.c` — CLI driver
- `share/wkb-blocklist.txt` — 53-entry default blocklist
- `tests/fixtures/` — upload form and server for the t7 test
- `src/null_view_backend.c` — dead WPE experiment, kept for the record

## Known limits

- File picker cannot pre-populate `input.files` from the embedder; automation
  must trigger the picker via a synthetic click. This is a WebKit API limit.
- Asset blocking classifies by URI extension, not by the resource type WebKit
  assigned, because `WebKitURIRequest` in 2.50.6 exposes no `get_resource_type()`.
  Extensionless CDN URLs will slip through.
- The default context is ephemeral, so multi-step logins need `--persist`.

## Upstream limitations that shaped the design

Both verified against the installed headers rather than assumed:

- **No content-filter API.** `WebKitUserContentFilter` has no public
  constructor in 2.50.6 (confirmed with `nm` on the shipped library), so the
  blocklist is enforced by cancelling subresource loads instead of a JSON
  rule set.
- **No resource type on the request.** `resource-load-started` passes a
  `WebKitURIRequest` with no resource-type accessor.

## Legal scope

Intended for your own identity and legitimate automation: testing your own
sites, research you are authorised to conduct, and workflows where you own the
account.

Not intended for CAPTCHA or MFA defeat, rotating identities to evade rate
limits, or scraping that ignores a site's terms of service. HiQ v. LinkedIn
settled the CFAA question for scraping public pages, but the ToS claim still
failed there, so terms-of-service exposure is live civil risk even where the
CFAA is not. That is a judgement call about your specific use, not a technical
one.