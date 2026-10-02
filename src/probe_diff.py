#!/usr/bin/env python3
"""Compare a wkbrowser probe run against tests/expected_fingerprint.json.

Usage:
    probe_diff.py --actual FILE [--expected FILE] [--update] [--ignore KEY,KEY]

Exits non-zero when any field differs, so CI fails on a fingerprint
regression. Volatile fields (inner size, tzOffset) are excluded by default
because they legitimately change with the viewport and host clock.
"""
import argparse, json, sys

# Fields that are expected to move between runs and therefore cannot be
# asserted on. Everything else must match exactly.
VOLATILE = {
    "inner",     # window.innerWidth/Height is 0 until a real layout pass
    "tzOffset",  # host clock dependent
    "tz",        # host timezone dependent
    "languages", # locale dependent
}

PROFILE_MANAGED = {          # set by the profile, asserted separately
    "ua", "cores", "platform", "screen", "colorDepth", "dpr",
    "webdriver", "notif", "webgl",
}


def load(p):
    with open(p) as f:
        return json.load(f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--actual", required=True)
    ap.add_argument("--expected", default="tests/expected_fingerprint.json")
    ap.add_argument("--update", action="store_true",
                    help="overwrite the baseline with --actual")
    ap.add_argument("--ignore", default="", help="extra keys to ignore")
    args = ap.parse_args()

    if args.update:
        import shutil
        shutil.copy(args.actual, args.expected)
        print(f"baseline updated: {args.expected}")
        return 0

    try:
        exp, act = load(args.expected), load(args.actual)
    except (OSError, json.JSONDecodeError) as e:
        print(f"FAIL cannot load: {e}", file=sys.stderr)
        return 2

    ignore = VOLATILE | {k for k in args.ignore.split(",") if k}

    diffs, missing = [], []
    for k, v in sorted(exp.items()):
        if k in ignore:
            continue
        if k not in act:
            missing.append(k)
        elif act[k] != v:
            diffs.append((k, v, act[k]))
    for k in sorted(act):
        if k not in exp and k not in ignore:
            diffs.append((k, "<absent>", act[k]))

    print(f"probed {len(act)} vectors; asserting {len(exp) - len(ignore & set(exp))}")
    print(f"ignored as volatile: {', '.join(sorted(ignore)) or '(none)'}")

    if missing:
        print(f"\nMISSING ({len(missing)}): " + ", ".join(missing))
    if diffs:
        print(f"\nDIFFERENCES ({len(diffs)}):")
        for k, was, now in diffs:
            print(f"  {k}\n     expected: {was!r}\n     actual:   {now!r}")

    if not diffs and not missing:
        print("\nOK fingerprint matches baseline")
        return 0
    print(f"\nFAIL {len(diffs)} difference(s), {len(missing)} missing")
    return 1


if __name__ == "__main__":
    sys.exit(main())