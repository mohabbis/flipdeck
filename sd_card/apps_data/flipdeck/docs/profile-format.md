# FlipDeck Profile Format

Profiles are JSON files stored in `apps_data/flipdeck/profiles/`.

Version 2 profiles use `commands`. Legacy profiles that use `actions` and
`confirm` are still accepted by the desktop helper and web installer.

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

Use `extends` to inherit commands from another profile in the same directory:

```json
{
  "name": "My Git",
  "extends": "git.json",
  "commands": [
    {
      "label": "Git Log",
      "type": "text",
      "value": "git log --oneline\n"
    }
  ]
}
```

Validate a profile before copying it to the Flipper SD card:

```bash
flipdeck profile:validate sd_card/apps_data/flipdeck/profiles/git.json
```
