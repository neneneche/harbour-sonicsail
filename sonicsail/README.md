# Soundsail

SoundCloud client for **Sailfish OS**.

![icon](icons/172x172/harbour-soundsail.png)

## Features

- **Home** feed
- **Search**
- **Likes**
- **Playlists**
- Now playing with seek, **shuffle**, **repeat** (off / all / one)
- Long-press on a track: open, play next, like / unlike, download
- **MPRIS** lock-screen / system media controls
- Download tracks to `Music/Soundsail/` (or app cache)
- optional: **Auto-download** played tracks after 60%


## Build

Requires [Sailfish SDK](https://docs.sailfishos.org/) with `SailfishOS-5.1.0.11-aarch64` (or later).

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

- [yt-dlp](https://github.com/yt-dlp/yt-dlp) SoundCloud extractor — client_id scraping, API and stream behavior
- [inkitori/soundcloud-desktop](https://github.com/inkitori/soundcloud-desktop) — login via embedded browser + cookie harvest, like/repost/follow endpoint shapes, writes through a hidden WebView (bot-protection workaround), batch `/tracks?ids=` hydration
- [Jomvet-lovers/soundcloud-desktop](https://github.com/Jomvet-lovers/soundcloud-desktop) — "Made for you" shelf (`/mixed-selections`) and system-playlist URNs
- [maid-zone/soundcloak](https://github.com/maid-zone/soundcloak) — discover/mixed-selections & charts endpoints, waveform JSON, web-app endpoint map
- [jaylinski/kodi-addon-soundcloud](https://github.com/jaylinski/kodi-addon-soundcloud) — `calculated_artwork_url`, the ready-made mosaic cover for mixes (also used by lbrndnr/SoundCloud and SpotiFlyer)
- [NotTobi/soundcloud-dl](https://github.com/NotTobi/soundcloud-dl) — waveform `.png`→`.json` peaks pattern
- [sailfish-components-webview](https://github.com/sailfishos/sailfish-components-webview) / [sailfish-browser](https://github.com/sailfishos/sailfish-browser) (Jolla) — embedded WebView API and the `clear-private-data` cookie-clearing mechanism
- [Didomi Web SDK docs](https://developers.didomi.io/) and [crawl4ai](https://github.com/unclecode/crawl4ai) — cookie-consent banner handling research
- [SubMusic](https://github.com/) and other Sailfish media apps — playback patterns on Sailfish OS
- [Sailfish OS](https://sailfishos.org) / Jolla — Silica UI, icon shape guidance, QtMultimedia
- SoundCloud — music and public web API

## License

See repository license (BSD-3-Clause unless otherwise noted).
