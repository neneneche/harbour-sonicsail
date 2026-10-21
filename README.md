# Soundsail

SoundCloud client for **Sailfish OS**.

![icon](icons/172x172/harbour-soundsail.png)

## Features

- **Home** — your SoundCloud feed (new tracks and reposts from artists you follow)
- **Search**
- **Likes**
- **Playlists**
- Now playing with seek, **shuffle**, **repeat** (off / all / one)
- Long-press on a track: open, play next, like / unlike, download
- **MPRIS** lock-screen / system media controls
- Download tracks to `Music/Soundsail/` (or app cache)
- optional: **Auto-download** played tracks after 60%


## Build

Requires [Sailfish SDK](https://docs.sailfishos.org/) with `SailfishOS-5.1.0.11-aarch64` (or later) tooling.

```bash
sfdk config --global target=SailfishOS-5.1.0.11-aarch64
cd harbour-soundsail
sfdk build-init
sfdk build
```

### Install on device

```bash
devel-su pkcon install-local harbour-soundsail-*.rpm
```

Permissions: `Internet`, `Audio`, `Music` (Sailjail).

## Backend

Uses the **public SoundCloud web API** (`api-v2.soundcloud.com`):

- A `client_id` is discovered at runtime from SoundCloud’s web player (no developer app / paid plan required).
- Streams come from `media.transcodings` (HLS AAC / progressive).
- Optional sign-in: in-app sign-in on SoundCloud’s own page (session cookie harvested automatically), or paste an `oauth_token` cookie for likes / library. No developer app or Artist Pro subscription involved.

This is an unofficial client for personal use. Respect SoundCloud’s Terms of Use and always credit uploaders.

## Authors

- **Xiaomi MiMo v2.6 Pro**
- **neneneche**

## Credits

- [yt-dlp](https://github.com/yt-dlp/yt-dlp) SoundCloud extractor — API and stream behavior
- [SubMusic](https://github.com/) and other Sailfish media apps — playback patterns on Sailfish OS
- [Sailfish OS](https://sailfishos.org) / Jolla — Silica UI, icon shape guidance, QtMultimedia
- SoundCloud — music and public web API

## License

See repository license (BSD-3-Clause unless otherwise noted).
