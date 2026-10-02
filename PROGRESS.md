# Progress log

## Goal state

Part B (embedder-level scraping features) is **complete and CI-verified**.
Part A (WebKit source build) is still unproven.

## Verified working, with measurements

| Item | Evidence |
|---|---|
| t7 file picker API | `src/test_t7.sh` 4/4, green in CI run 37065936538 |
| P2-1 ephemeral context | default `ephemeral`, `--persist` opts out |
| P2-2 asset blocking | `blocked=4 allowed=3`; control `blocked=0 allowed=7` |
| P2-3 domain blocklist | 53 entries parsed, GA request blocked |
| P2-5 fingerprint probe | 22 vectors, 18 asserted, regression diff exits non-zero |
| CI test-embedder | run 37065936538 all 7 steps success |

## Part A status: NOT DONE

The stripped WebKit has never compiled. Run 37060005130 is still in progress.
As of the last check the error had progressed from CMake configure failures to
Clang compile failures, but no green run exists, so **the RSS delta versus the
414 MB unstripped baseline is still unknown** and must not be quoted.

`USE_GSTREAMER=OFF` remains unproven. Upstream PR #61274 documents it being
forced back to `TRUE`; the workflow asserts it against `CMakeCache.txt` and
checks `readelf -d | grep -i gst`, but neither has run on a real library.

## Defects found and fixed since the last pause

These were all found by CI running against a clean checkout, which local runs
had been masking:

- **Fixtures were gitignored.** `tests/fixtures/` now holds the upload form and
  server, so a fresh clone can actually run the tests.
- **`upload_server.py` served the wrong root.** One `dirname` too few, so every
  page load 404'd and t7 silently tested nothing.
- **The upload form hardcoded port 8077** while the test runs on 8081.
- **t7 asserted `files[]` after `form.submit()`**, which navigates away, so the
  assertion ran against a fresh document and saw no input. Reordered.
- **`log/` is gitignored**, so every test script died on its first redirect.
- **`probe_diff` treated the UA as exact-match**, but it embeds the host
  distro token. Now structurally asserted.
- **The apt fallback acted on an empty parse**, fuzzy-matching
  `libavahi-glib-dev` and burning all 8 configure attempts.

## Process changes made to stop the CI grind

- Workflow `run` blocks are `bash -n` validated before every push, and CI now
  lints every workflow for the same thing.
- CI flags any step piped to `tee` without `pipefail`; that check would have
  caught the bug where a failed `ninja` reported success.
- `build-webkit-stripped` is `workflow_dispatch` only. The `push` trigger was
  duplicating every dispatch and burning the free tier.
- WebKit is pinned to a commit SHA so failures reproduce.
- The expensive build is separate from `test-embedder`, which gates every push
  in about two minutes.