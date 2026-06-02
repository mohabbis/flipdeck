# FlipDeck Desktop Helper

Command-line tools for creating, validating, previewing, auditing, sharing, and syncing FlipDeck profiles.

## Install

```bash
cd desktop_helper
npm ci
npm run build
```

During development, run commands through `ts-node`:

```bash
npm start -- profile validate ../sd_card/apps_data/flipdeck/profiles/git.json
```

After building, the published binary name is `flipdeck`.

## Profile Format

Profiles are JSON files stored on the Flipper SD card under:

```text
sd_card/apps_data/flipdeck/profiles/
```

The v2 format uses `commands`:

```json
{
  "name": "Git",
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

Supported icons are `git`, `docker`, `node`, `python`, `aws`, `vscode`, `system`, `snippets`, and `presentation`.

Legacy profiles that use `actions` and `confirm` are still accepted. They are normalized to `commands` with `delay_ms: 100` and `confirmation_required` based on `confirm`.

Profiles can inherit commands from another profile in the same directory:

```json
{
  "name": "Feature Work",
  "extends": "base-git.json",
  "commands": [
    {
      "label": "Checkout",
      "type": "text",
      "value": "git checkout -b "
    }
  ]
}
```

Inherited commands are loaded first, then child commands are appended.

## Commands

The headings below use the product shorthand from the feature spec, such as
`profile:validate`. The current Commander CLI runs these as nested commands,
such as `flipdeck profile validate`.

### `profile:validate <file>`

Validate a profile against the schema and safety rules.

```bash
flipdeck profile validate ./profiles/git.json
```

Validation reports schema paths such as `commands.0.type`. Invalid JSON errors include file, line, and column details when Node can provide the parse position.

### `profile:new --interactive`

Create a profile with an interactive prompt.

```bash
flipdeck profile new --interactive --output ./profiles/custom.json
```

Without `--interactive`, this writes a small starter profile. Output defaults to the configured profiles directory.

### `profile:edit <file>`

Validate, normalize, and format a profile in place.

```bash
flipdeck profile edit ./profiles/custom.json
```

Use this to migrate accepted legacy fields into the v2 shape for a single file.

### `profile:preview <file>`

Render an ASCII mockup of the Flipper screen for the profile.

```bash
flipdeck profile preview ./profiles/git.json
```

The preview truncates labels to fit the 128x64 monochrome UI constraints.

### `profile:audit <dir>`

Scan every JSON profile in a directory for risky text commands.

```bash
flipdeck profile audit ./profiles
```

The audit flags patterns such as `sudo`, `rm -rf`, `curl | sh`, disk formatting commands, and common credential names.

### `profile:migrate --from=v1`

Convert legacy `actions` profiles in a directory to v2 `commands`.

```bash
flipdeck profile migrate --from=v1 --dir ./profiles
```

Only `--from=v1` is currently supported.

### `profile:share <file>`

Print a profile preview URL and QR code.

```bash
flipdeck profile share ./profiles/git.json
flipdeck profile share ./profiles/git.json --url https://flipdeck-production.up.railway.app/preview
```

The default share base URL is `https://flipdeck-production.up.railway.app/preview`. You can also set `shareBaseUrl` in `~/.flipdeck/config.json`.

### `profile:sync --push`

Upload local profiles to a GitHub Gist.

```bash
GITHUB_TOKEN=ghp_example flipdeck profile sync --push --dir ./profiles
```

If a gist has already been saved in `~/.flipdeck/config.json`, pushes update that gist. Otherwise the command creates a private gist and saves its ID.

### `profile:sync --pull <gist>`

Download JSON profiles from a GitHub Gist.

```bash
GITHUB_TOKEN=ghp_example flipdeck profile sync --pull 0123456789abcdef --dir ./profiles
```

The token can be passed with `--token` instead of `GITHUB_TOKEN`.

## Config

User config is stored at `~/.flipdeck/config.json`:

```json
{
  "gistId": "0123456789abcdef",
  "shareBaseUrl": "https://flipdeck-production.up.railway.app/preview"
}
```

Sync is optional. Profile validation, creation, preview, audit, and migration all work offline.

## Tests

```bash
cd desktop_helper
npm test
npm run build
```
