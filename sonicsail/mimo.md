# mimo.md — project rules


## Build & release

- Build with `sfdk build` against `SailfishOS-5.1.0.11-aarch64`; output RPM in `RPMS/`.
- Version and release are defined in `rpm/harbour-soundsail.yaml`.
- Bump `Version` or `Release` for every build installed on a device:
  package managers skip upgrades with an identical version-release.
- Icon source: `icons/172x172/`; derive 86/108/128 with `sips`.
- Build outputs (`RPMS/`, `*.o`, `Makefile`, `moc_*`, compiled binaries) are
  not committed.
- The repository contains source only; credits are listed in `README.md`.


## Authentication

- No developer app and no `client_secret`: anonymous requests use the public
  web-player `client_id` scraped at runtime (`ClientIdResolver`).
- A logged-in session is an `oauth_token` cookie obtained by signing in on
  `soundcloud.com` in the embedded browser; it is sent as
  `Authorization: OAuth <token>` to `api-v2.soundcloud.com`.
- Token validation: only HTTP 401 invalidates the stored session. 403 means
  bot protection and must never delete the saved token.
- Write operations (like, repost, follow) go through `WebWriter`, which runs
  XHRs inside a hidden browser page — plain HTTP writes are rejected.
- `WebWriter` creates its WebView lazily on first use; the app must never run
  two WebViews at once (it breaks the embedded engine).


## Embedded sign-in browser

- The page is left untouched: no scripts to auto-dismiss cookie banners, no
  DOM changes, no reload loops. Dismissing banners is the user's action.
- `WebView.runJavaScript` scripts must end with a top-level `return`.
- Session harvesting polls both the profile cookie database and
  `document.cookie`, then validates via `GET /me`.


## SoundCloud api-v2 (verified 2026)

- Base `https://api-v2.soundcloud.com` with `client_id` query parameter;
  v1 `api.soundcloud.com` is retired.
- Playlist tracks: `GET /playlists/{id}/tracks` is removed (404). Use
  `GET /playlists/{id}?representation=full` — it embeds a few full tracks and
  id-only stubs; hydrate stubs in batches of 50 via `GET /tracks?ids=…`.
- `/me/library/*` is removed. Owned playlists: `/users/{id}/playlists`;
  liked playlists: `/users/{id}/playlist_likes`.
- Home feed: `GET /stream`. Personalised mixes shelf: `GET /mixed-selections`.
- System playlists (mixes, stations, chart sets): `GET /system-playlists/{urn}`
  — only the full URN form `soundcloud:system-playlists:…` is accepted.
- Mix artwork: use `calculated_artwork_url` when `artwork_url` is null
  (SoundCloud serves the 2x2 collage pre-rendered).
- Like/unlike: `PUT/DELETE /users/{userId}/track_likes/{trackId}`;
  repost: `PUT/DELETE /me/track_reposts/{id}`;
  follow: `POST/DELETE /me/followings/{id}`.
- Old discover/chart permalinks (`discover/sets/charts-*`) return 404.


## UI

- Sailfish Silica / QtQuick only; follow existing page patterns and theme
  colors.
- The "Settings" menu entry always opens the account/settings page, both
  signed in and signed out; Experimental and About stay reachable logged out.
- The waveform seek bar is disabled (commented out) until its peaks loading
  is verified working on device.
