# shaslyk firmware releases

This repo is used as a **firmware-distribution** repository: GitHub builds `firmware.bin` and publishes it in GitHub **Releases** for ESP32 OTA.

Repository: [steel0rat/shaslyk](https://github.com/steel0rat/shaslyk)

## How it works

- Workflow runs on push of tags that match `v*`.
- It checks out the **firmware source** repository at the same tag/commit (see variables below) and runs PlatformIO.
- It uploads `firmware.bin` to a GitHub Release for that tag.
- OTA in firmware expects a release asset named `firmware.bin`.

## One-time setup (GitHub)

In `shaslyk`, open `Settings` → `Secrets and variables` → `Actions` → `Variables` / `Repository secrets`.

### Variables (recommended)

- `SOURCE_REPO` — source repo with the PlatformIO project (`owner/name`).
- `PIO_ENV` — PlatformIO environment to build, for example: `lolin32`.

If `PIO_ENV` is empty, the workflow falls back to `lolin32`.

### Secrets (optional)

- `SOURCE_REPO_TOKEN` — a PAT (classic or fine-grained) that can **read** the source repository.
  - **Private** source repo: **required**
  - **Public** source repo: **not required** (the workflow uses `git clone https://github.com/...` without a token)

## Release flow

1. In the **source** repo, tag a release, for example: `v1.0.3` (and push the tag).
2. In this repo, create the **same** tag: `v1.0.3` (and push the tag) so the build checks out a consistent ref.
3. Wait for the `Build and Release Firmware` workflow to finish.
4. Open the GitHub `Releases` page and verify `firmware.bin` is attached.

### Manual test build

You can also run the workflow from `Actions` via `Run workflow` (it uploads an `artifact` if it is not a tag run).

## Notes

- The firmware’s `FIRMWARE_VERSION` should match the tag (e.g. `1.0.3` for tag `v1.0.3`).
