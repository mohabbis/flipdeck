# Installer Flow

The web installer exists to make FlipDeck feel like a simple device install instead of a repo checkout.

## User Flow

1. Open the deployed FlipDeck installer.
2. Choose the default profiles or a custom subset.
3. Download the install ZIP.
4. Extract the ZIP locally.
5. Drag `apps_data` onto the Flipper SD card root in qFlipper.
6. Launch FlipDeck from the Flipper apps menu.

## Install Pack Contract

The ZIP must include:

- `README-FIRST.txt`
- `apps_data/flipdeck/settings.json`
- `apps_data/flipdeck/profiles/*.json`
- `apps_data/flipdeck/snippets/*.txt`

The installer should not require credentials, GitHub access, or a network call after page load.

## Validation

From `web/`:

```sh
npm test
npm run lint
npm run build
```

The API tests verify the generated ZIP includes the expected SD card paths.
