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
| t8 anti-detection | partial | `webdriver` already false; UA/WebGL pending |
| t3/t6/t9 source build | blocked | needs >16 GB RAM host |
| t10 disk audit | partial | 200 KB used vs 10 GB cap |

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

```
--url URL          page to load (required)
--dump             print document.body.innerText after load
--probe            print a JSON fingerprint probe after load
--file PATH        register PATH as the file the picker returns
--screen WxH       fake screen dimensions
--geo LAT,LON      fake geolocation
--cores N          fake navigator.hardwareConcurrency
--memory GB        fake navigator.deviceMemory
--ua STRING        fake navigator.userAgent
--tz STRING        fake Intl timezone
```

## Example: verified fake-value run

```sh
$ xvfb-run -a ./dist/wkbrowser --url "data:text/html,<h1>probe</h1>" --probe \
    --screen 1920x1080 --cores 8 --geo 51.5074,-0.1278 --tz "America/New_York"
{"ua":"Mozilla/5.0 (X11; Linux x86_64) ... Safari/605.1.15","webdriver":false,
 "cores":8,"platform":"Linux x86_64","screen":[1920,1080,24],"dpr":1,
 "tz":"America/New_York","devices":"n/a"}
```

## Tests

```sh
./src/test_t7.sh    # file chooser + byte-exact upload verification
```

## Architecture

- `src/wkb.h` — profile struct + public API
- `src/wkb_profile.c` — synthetic-value injection (DOCUMENT-END user scripts)
- `src/wkb_files.c` — file registry / `run-file-chooser` handler
- `src/wkbrowser.c` — CLI driver
- `src/null_view_backend.c` — dead WPE experiment, kept for the record

## Known limits

- Peak RSS ~414 MB unstripped. Getting to "super light" needs a source build
  with media/GPU paths removed, which needs a host with >16 GB RAM.
- `navigator.webdriver` is already false; UA string is still the stock
  Safari-60-era value and needs a coherent modern profile.
- WebGL renderer string still leaks the real Mesa stack (needs engine patch).
- File picker cannot pre-populate `input.files` from the embedder; automation
  must trigger the picker via a synthetic click. This is a WebKit API limit.