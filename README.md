# Soundsail

SoundCloud client for **Sailfish OS**.

![icon](icons/172x172/harbour-soundsail.png)

## Features

- **Playlists** tab — your created playlists, library, and saved (liked) playlists
- **Search** tracks
- **Likes**
- Now playing with seek, **shuffle**, **repeat** (off / all / one)
- Long-press on a track: open, play next, like / unlike, download
- **MPRIS** lock-screen / system media controls
- Download tracks to `Music/Soundsail/` (or app cache)
- **Auto-download** played tracks after 60% (optional, in Settings)


## Build

Requires [Sailfish SDK](https://docs.sailfishos.org/) with `SailfishOS-5.1.0.11-aarch64` (or later) tooling.

```bash
sfdk config --global target=SailfishOS-5.1.0.11-aarch64
cd harbour-soundsail
sfdk build-init
sfdk build
```

RPM: `RPMS/harbour-soundsail-<version>-1.aarch64.rpm`

### Install on device

```bash
devel-su pkcon install-local harbour-soundsail-*.rpm
```

Permissions: `Internet`, `Audio`, `Music` (Sailjail).

## Backend

Uses the **public SoundCloud web API** (`api-v2.soundcloud.com`):

- A `client_id` is discovered at runtime from SoundCloud’s web player (no developer app / paid plan required).
- Streams come from `media.transcodings` (HLS AAC / progressive). Many tracks are HLS-only.
- Optional sign-in: import browser cookies for the Sailfish browser, or paste an `oauth_token` cookie for likes / library.

This is an unofficial client for personal use. Respect SoundCloud’s Terms of Use and always credit uploaders.

## Authors

- **neneneche**
- **Xiaomi MiMo v2.6 Pro**

## Credits

- [yt-dlp](https://github.com/yt-dlp/yt-dlp) SoundCloud extractor — API and stream behavior
- [SubMusic](https://github.com/) and other Sailfish media apps — playback patterns on Sailfish OS
- [Sailfish OS](https://sailfishos.org) / Jolla — Silica UI, icon shape guidance, QtMultimedia
- SoundCloud — music and public web API

## Links

- Repository: https://github.com/neneneche/harbour-sonicsail

## License

See repository license (BSD-3-Clause unless otherwise noted).
