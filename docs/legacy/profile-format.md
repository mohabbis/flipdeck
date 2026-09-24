# Profile Format

Profiles are JSON files stored at `apps_data/flipdeck/profiles/` on the Flipper SD card.

Version 2 profiles use `commands`. Legacy profiles that use `actions` and `confirm` are still accepted by the desktop helper and web installer.

```json
{
  "name": "Git",
  "id": "git",
  "description": "Git workflow shortcuts",
  "icon": "git",
  "commands": [
    {
      "label": "Git Status",
      "type": "text",
      "value": "git status\n",
      "delay_ms": 100,
      "confirmation_required": true
    }
  ]
}
```

## Schema

The bundled JSON schema lives at:

```text
sd_card/apps_data/flipdeck/schemas/profile.v2.schema.json
```

Use it as the source of truth for profile validation. Supported command types are:

- `text`
- `key`
- `key_combo`

## Validation

Validate a profile before copying it to the SD card:

```sh
cd desktop_helper
npm test
npm start -- profile:validate ../sd_card/apps_data/flipdeck/profiles/git.json
```

The validator checks schema shape and scans text commands for dangerous or credential-like patterns.
