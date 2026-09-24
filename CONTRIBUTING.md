# Contributing to FlipDeck

Start with `ARCHITECTURE.md`, then `docs/protocol.md`. `CLAUDE.md` has the
commands and the architecture rules. They apply to humans too.

## Setup

- **Mac app:** macOS 14+ with Xcode 15+ (or just the command-line tools).
  `cd mac && swift test`, then `scripts/build-app.sh`.
- **Flipper app:** Python 3.8+ and `pip install ufbt`, then `ufbt` at the repo root.
  `make -C src/tests/host run` runs the host tests (any C compiler).

## Before opening a PR

```sh
(cd mac && swift build && swift test)
make -C src/tests/host run
ufbt                                  # or scripts/check_flipper_sdk.sh
```

If you touch the wire protocol, update `docs/protocol.md`, both implementations,
and the golden vectors (`python3 scripts/gen_protocol_vectors.py`) in the same PR.

## Principles

1. Reliability over features.
2. Real data only. If something can't be detected reliably, say it's unsupported.
3. The Flipper never gets a way to run arbitrary commands. New actions are new
   `ActionKind` cases with validation in `ActionExecutor`.
4. Low overhead: bounded scans, timeouts on every external command, no busy loops.
5. Native macOS behavior and a restrained, information-dense UI.

## License

By contributing you agree that your contributions are licensed under the MIT
License.
