# Proposed CI workflows

These files are meant for `.github/workflows/`. The session that wrote them
couldn't push workflow files (its GitHub token lacks the `workflow` scope), so
a maintainer has to install them:

```sh
cp ci/test.yml .github/workflows/test.yml
cp ci/build-fap.yml .github/workflows/build-fap.yml
git add .github/workflows && git commit -m "CI: build and test the Mac app and Flipper app"
```

Then delete this directory.

| Job | What it proves |
|---|---|
| `mac-macos` | The whole Swift package compiles on macOS, including the SwiftUI app, the CoreBluetooth transport, Keychain, and IOKit, which can't compile on Linux. Runs all tests and builds `FlipDeck.app` as an artifact. |
| `mac-core-linux` | The platform-independent core builds and passes its tests on Linux. |
| `flipper-host` | The C protocol and state code pass their tests under ASan/UBSan, including the golden vectors shared with Swift. |
| `flipper-fap` | `flipdeck.fap` builds with uFBT against the official release SDK, on every PR. |

Until they're installed, the existing workflows still cover `flipper-host`
(same command), and `build-fap.yml` builds the new `.fap` on master. Nothing
builds or tests the Swift code in CI until then.
