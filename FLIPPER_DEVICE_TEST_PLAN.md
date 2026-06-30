# FlipDeck Flipper Device Test Plan

This document is the step-by-step on-device test path for validating FlipDeck with `ufbt` and a real Flipper Zero. It is written for the practical case: build the `.fap`, put it on the device, stage test data on the SD card, run the app from the Flipper UI, and confirm the behavior on a host computer.

The goal is not to prove that every branch of the C code is perfect. That is what unit tests, static checks, and code review are for, because apparently one test method cannot carry an entire engineering civilization. This plan verifies that the app actually works where it matters: on the device, using the real SD card paths, real buttons, real USB HID behavior, and real user confirmation.

## Scope

This plan covers:

1. Local `ufbt` setup.
2. Clean app build.
3. Optional launch through `ufbt launch` when supported by the connected device setup.
4. Manual `.fap` installation through qFlipper or SD card copy.
5. Required SD card data layout.
6. On-device navigation and profile loading.
7. USB HID command execution into a safe host target.
8. Safety and failure-mode checks.
9. Regression checklist before release.

This plan does not cover:

- Web installer browser QA.
- Full security review.
- CI configuration review.
- Malicious payload testing against real systems.
- Any test that sends commands into shells, terminals, production apps, cloud consoles, password fields, or admin prompts.

Use a plain text editor as the host target. Not Terminal. Not PowerShell. Not a browser address bar. Humanity has suffered enough.

## Repo assumptions

FlipDeck is built as an external Flipper app with this app identity:

- App ID: `flipdeck`
- App name: `FlipDeck`
- Category: `Tools`
- Expected app file: `dist/flipdeck.fap`
- Expected installed app path on Flipper: `/ext/apps/Tools/flipdeck.fap`
- Expected data directory: `/ext/apps_data/flipdeck/`
- Expected profiles directory: `/ext/apps_data/flipdeck/profiles/`
- Expected snippets directory: `/ext/apps_data/flipdeck/snippets/`
- Expected settings file: `/ext/apps_data/flipdeck/settings.json`

## Required hardware and software

### Hardware

- Flipper Zero.
- Flipper SD card inserted and working.
- USB-C data cable. Charge-only cables are a classic little prank from the hardware gods, so verify the cable supports data.
- Test host computer running macOS, Windows, or Linux.

### Software

- Python 3.11 or newer.
- `pip`.
- `ufbt`.
- qFlipper, optional but useful for copying files and confirming the device is detected.
- A plain text editor on the host computer.

## Safety rules for every test run

1. Test only on a computer you own or are authorized to use.
2. Keep the focused host app as a plain text editor.
3. Do not test in a shell, terminal, browser, password prompt, admin prompt, or production app.
4. Do not store credentials, tokens, secrets, destructive commands, or personal data in test profiles.
5. Keep commands visibly harmless, such as `echo FlipDeck test`, `git status`, or typed comments.
6. Disconnect the Flipper immediately if it sends unexpected input.
7. Record failures before retrying so the bug does not evaporate into folklore.

## Phase 1: prepare the local environment

1. Clone the repo if needed:

   ```sh
   git clone https://github.com/mohabbis/flipdeck.git
   cd flipdeck
   ```

2. Confirm the expected project files exist:

   ```sh
   test -f application.fam
   test -d src
   test -d assets
   ```

3. Create and activate a Python virtual environment:

   ```sh
   python3 -m venv .venv
   . .venv/bin/activate
   ```

4. Upgrade packaging tools:

   ```sh
   python -m pip install --upgrade pip setuptools wheel
   ```

5. Install `ufbt`:

   ```sh
   python -m pip install --upgrade ufbt
   ```

6. Confirm `ufbt` is available:

   ```sh
   ufbt --help
   ```

Expected result:

- `ufbt --help` prints command usage.
- No Python import or executable-not-found errors appear.

## Phase 2: clean build with ufbt

1. Remove previous build output:

   ```sh
   rm -rf dist build .ufbt
   ```

2. Build the app:

   ```sh
   ufbt
   ```

3. Confirm the `.fap` exists:

   ```sh
   test -f dist/flipdeck.fap
   ls -lh dist/flipdeck.fap
   ```

Expected result:

- Build completes without compiler errors.
- `dist/flipdeck.fap` exists.
- The file size is non-zero.

Failure notes:

- If `ufbt` fails while downloading or resolving the SDK, retry once on a stable network.
- If compilation fails, capture the first compiler error and the source file path. Do not chase every downstream error like a raccoon in a server room.

## Phase 3: connect and detect the Flipper

1. Connect the Flipper Zero over USB.
2. Unlock the Flipper if needed.
3. Confirm the Flipper is visible to the host.

Optional qFlipper check:

1. Open qFlipper.
2. Confirm the device appears.
3. Confirm the SD card browser opens.

Optional CLI check:

```sh
ufbt cli
```

Expected result:

- The Flipper is detected by qFlipper or `ufbt cli`.
- The SD card is accessible.

If detection fails:

1. Try another USB port.
2. Try another USB-C data cable.
3. Restart qFlipper.
4. Reboot the Flipper.
5. Confirm the SD card is inserted and mounted.

## Phase 4: install the app file

Use either Path A or Path B.

### Path A: install or launch with ufbt

Use this path when the local `ufbt` setup can talk to the connected Flipper.

1. Build and launch:

   ```sh
   ufbt launch
   ```

2. Watch the Flipper screen.
3. Confirm FlipDeck opens or is transferred and launched.

Expected result:

- The app launches on the Flipper, or `ufbt` reports a clear transfer/launch result.

If `ufbt launch` is unavailable or unreliable for the local setup, use Path B. The point is to test FlipDeck, not to perform a spiritual ritual around one command.

### Path B: copy `.fap` manually

1. Open qFlipper.
2. Open the SD card browser.
3. Navigate to:

   ```text
   /ext/apps/Tools/
   ```

4. Copy the built app file:

   ```text
   dist/flipdeck.fap
   ```

   to:

   ```text
   /ext/apps/Tools/flipdeck.fap
   ```

5. Safely close the SD browser or eject if using direct SD mounting.
6. On the Flipper, go to:

   ```text
   Apps -> Tools
   ```

7. Confirm `FlipDeck` appears in the list.

Expected result:

- `FlipDeck` appears under Tools.
- Opening it does not crash the device.

## Phase 5: stage the required SD card data

Create the app data directory on the Flipper SD card:

```text
/ext/apps_data/flipdeck/
```

Inside it, create:

```text
/ext/apps_data/flipdeck/settings.json
/ext/apps_data/flipdeck/profiles/
/ext/apps_data/flipdeck/snippets/
/ext/apps_data/flipdeck/logs/
```

Minimum settings file:

```json
{
  "send_delay_ms": 30,
  "confirm_before_send": true,
  "theme": "default"
}
```

Minimum safe test profile:

```json
{
  "name": "Device Test",
  "id": "device_test",
  "description": "Safe on-device validation profile",
  "commands": [
    {
      "label": "Type harmless line",
      "type": "text",
      "value": "FlipDeck device test OK\n",
      "confirmation_required": true
    },
    {
      "label": "Type comment",
      "type": "text",
      "value": "# FlipDeck safe comment test\n",
      "confirmation_required": true
    },
    {
      "label": "Press Enter",
      "type": "key",
      "value": "ENTER",
      "confirmation_required": true
    },
    {
      "label": "Select All",
      "type": "key_combo",
      "value": "CTRL+A",
      "confirmation_required": true
    }
  ]
}
```

Save it as:

```text
/ext/apps_data/flipdeck/profiles/device_test.json
```

Expected result:

- The Flipper SD card contains the expected `apps_data/flipdeck` structure.
- `device_test.json` is valid JSON.
- The profile uses only harmless text and editor-safe key actions.

## Phase 6: first app launch on device

1. Disconnect and reconnect the Flipper if the SD card was modified externally.
2. On the Flipper, open:

   ```text
   Apps -> Tools -> FlipDeck
   ```

3. Observe the initial screen.
4. Navigate the profile list.
5. Select `Device Test`.

Expected result:

- FlipDeck launches without a crash.
- The app reads `/ext/apps_data/flipdeck/profiles/device_test.json`.
- The `Device Test` profile appears by name.
- The command labels appear correctly.
- Button navigation works.

Record:

- Firmware version.
- Build date or commit SHA tested.
- Whether the profile loaded on first launch.
- Any missing directory or parsing error shown on device.

## Phase 7: USB HID safe execution test

1. On the host computer, open a plain text editor.
2. Create a blank document.
3. Click inside the document so it has focus.
4. Connect the Flipper over USB if not already connected.
5. On the Flipper, open FlipDeck.
6. Open the `Device Test` profile.
7. Select `Type harmless line`.
8. Confirm the send action on the Flipper.
9. Watch the text editor.

Expected result:

The host text editor receives:

```text
FlipDeck device test OK
```

Acceptance criteria:

- Nothing is sent before explicit confirmation.
- Text appears only in the focused editor.
- Newline behavior works.
- The app remains responsive after sending.

Repeat for:

- `Type comment`
- `Press Enter`
- `Select All`

For `Select All`, use a scratch text document only. Confirm that the text becomes selected. Do not press Delete afterward unless testing editor behavior intentionally.

## Phase 8: confirmation and cancellation behavior

For each command in the test profile:

1. Select the command.
2. When confirmation appears, cancel or go back.
3. Confirm that no input is sent to the host.
4. Select the same command again.
5. Confirm it.
6. Confirm that input is sent exactly once.

Expected result:

- Cancel means cancel. A shocking standard, but a useful one.
- OK means one send, not repeated sends.
- Back navigation returns to the previous menu without sending.

## Phase 9: profile parsing checks

Create these additional profiles one at a time and confirm the app handles them safely.

### Empty command list

Path:

```text
/ext/apps_data/flipdeck/profiles/empty.json
```

Content:

```json
{
  "name": "Empty",
  "id": "empty",
  "commands": []
}
```

Expected result:

- App does not crash.
- App shows an empty state or a safe no-command state.

### Invalid JSON

Path:

```text
/ext/apps_data/flipdeck/profiles/bad_json.json
```

Content:

```json
{
  "name": "Bad JSON",
  "id": "bad_json",
  "commands": [
```

Expected result:

- App does not crash.
- Invalid profile is skipped or shown as an error.
- Other valid profiles still load.

### Unsupported command type

Path:

```text
/ext/apps_data/flipdeck/profiles/unsupported_type.json
```

Content:

```json
{
  "name": "Unsupported Type",
  "id": "unsupported_type",
  "commands": [
    {
      "label": "Unsupported",
      "type": "magic",
      "value": "nope",
      "confirmation_required": true
    }
  ]
}
```

Expected result:

- App does not crash.
- Unsupported command is blocked, skipped, or shown as unsupported.
- No USB input is sent for unsupported command types.

## Phase 10: missing path checks

Run these checks with the app closed, then relaunch FlipDeck each time.

### Missing profiles directory

1. Rename:

   ```text
   /ext/apps_data/flipdeck/profiles/
   ```

   to:

   ```text
   /ext/apps_data/flipdeck/profiles_backup/
   ```

2. Launch FlipDeck.

Expected result:

- App does not crash.
- App shows a clear missing-profile or empty state.

Restore the directory before continuing.

### Missing settings file

1. Rename:

   ```text
   /ext/apps_data/flipdeck/settings.json
   ```

   to:

   ```text
   /ext/apps_data/flipdeck/settings_backup.json
   ```

2. Launch FlipDeck.

Expected result:

- App does not crash.
- App uses defaults or shows a safe settings warning.

Restore the file before continuing.

## Phase 11: persistence and relaunch checks

1. Close FlipDeck.
2. Reopen FlipDeck.
3. Confirm profiles still appear.
4. Power-cycle the Flipper.
5. Reopen FlipDeck.
6. Confirm profiles still appear.
7. Send the harmless line again into a text editor.

Expected result:

- Profile loading survives app relaunch.
- Profile loading survives device reboot.
- USB HID still works after reboot.

## Phase 12: release regression checklist

Before treating a `.fap` as release-ready, run this checklist:

- [ ] `ufbt` builds cleanly from a fresh checkout.
- [ ] `dist/flipdeck.fap` is produced.
- [ ] `.fap` installs to `/ext/apps/Tools/flipdeck.fap`.
- [ ] FlipDeck appears under `Apps -> Tools`.
- [ ] App launches without crashing.
- [ ] `/ext/apps_data/flipdeck/profiles/device_test.json` loads.
- [ ] Profile list displays expected profile names.
- [ ] Command list displays expected command labels.
- [ ] Text command sends only after confirmation.
- [ ] Key command sends only after confirmation.
- [ ] Key combo sends only after confirmation.
- [ ] Cancel/back sends nothing.
- [ ] Invalid JSON does not crash the app.
- [ ] Empty profile does not crash the app.
- [ ] Unsupported command type sends nothing.
- [ ] Missing profiles directory does not crash the app.
- [ ] Missing settings file does not crash the app.
- [ ] App remains responsive after at least 10 command sends.
- [ ] Device can exit the app cleanly.

## Suggested test matrix

| Area | Test | Expected result | Status |
| --- | --- | --- | --- |
| Build | `ufbt` from clean checkout | `dist/flipdeck.fap` exists | Not run |
| Install | Copy `.fap` to Tools | App appears under Tools | Not run |
| Launch | Open app on Flipper | No crash | Not run |
| SD read | Load `device_test.json` | Profile appears | Not run |
| Text send | Send harmless line | Text appears in editor | Not run |
| Key send | Send Enter | Editor receives Enter | Not run |
| Combo send | Send CTRL+A | Editor selects text | Not run |
| Cancel | Cancel confirmation | No host input | Not run |
| Invalid JSON | Load malformed profile | No crash | Not run |
| Missing paths | Remove profiles/settings temporarily | No crash | Not run |
| Relaunch | Reboot and reopen | Profiles still load | Not run |

## Bug report template

Use this format when a device test fails:

```text
## FlipDeck device test failure

Commit tested:
Firmware version:
ufbt version:
Install path used: ufbt launch / qFlipper / direct SD copy
Host OS:
USB cable verified as data cable: yes/no

### Failing phase

Phase number:
Step number:

### Expected behavior


### Actual behavior


### Reproduction steps

1.
2.
3.

### Device screen text or behavior


### Host app used for HID test


### Notes

```

## Pass criteria

A build passes the on-device `ufbt` test path when:

1. The app builds locally with `ufbt`.
2. The `.fap` installs or launches on a real Flipper Zero.
3. The app opens from `Apps -> Tools -> FlipDeck`.
4. The app reads profiles from `/ext/apps_data/flipdeck/profiles/`.
5. Commands are visible before execution.
6. Commands require explicit confirmation.
7. Safe text, key, and key combo commands work in a plain text editor.
8. Cancel/back sends no USB input.
9. Bad or missing profile data does not crash the app.
10. The app exits cleanly.

If all ten are true, the on-device path is healthy enough for the next release candidate. If any fail, fix that before adding features. Future-you deserves fewer mysteries.