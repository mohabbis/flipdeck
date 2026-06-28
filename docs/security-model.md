# FlipDeck Security Model

FlipDeck is a command automation tool, so safety is part of the product surface. The goal is useful shortcuts without stealth payload behavior, credential storage, or silent execution.

## Boundaries

- The Flipper app reads profile JSON from the SD card.
- Text commands are reviewed before sending.
- Key and key-combo actions are treated as interaction shortcuts, not shell text.
- The web installer packages known profiles into the expected SD card layout.
- The desktop helper validates profile structure and scans text commands before sync.

## Safety Rules

FlipDeck blocks or warns on patterns such as:

- `rm -rf`
- `sudo`
- `curl | sh` and `wget | sh`
- `mkfs`
- `dd if=`
- fork bombs
- credential markers such as `PASSWORD`, `TOKEN`, `API_KEY`, `SECRET`, and `PRIVATE_KEY`

These checks exist in the Flipper C profile manager, the web installer's command audit, and the desktop helper validation path.

## Required Product Behavior

- Show the action label and value before USB send.
- Require confirmation for text commands by default.
- Keep default profiles safe and reviewable.
- Never store access tokens or passwords in profiles.
- Keep downloaded install packs transparent: users should be able to inspect every JSON file before copying to the SD card.

## Review Checklist

- Does this change add a new way to send USB input?
- Does it preserve explicit confirmation before meaningful commands?
- Does it keep profile JSON readable and editable?
- Does it avoid network or credential requirements for the installer?
- Are safety tests updated when rules change?
