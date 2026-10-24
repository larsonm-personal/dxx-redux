# Minimal landing page

Copy `config.example.json` to `config.local.json`, fill in the title and links, then run `pwsh server/landing/build.ps1` from the repository root. Deploy only `dist/index.html` to the static web root, never the configuration or source directory.

The local config and generated `dist/` are gitignored. The template and renderer are intended for Git. Keep a private backup of the config. This prevents repository disclosure, **not visitor disclosure**: links in a public page are public, not access-controlled secrets. Google group/testing eligibility remains enforced by Google.

The current VPS web root is `/var/www/dxx-landing`, separate from the ACME challenge root and the fruitfacts website. Serve the landing page at `/`; reserve future `/ws` and `/api/` locations for matchmaking. No player-count requests are made yet.

The Android and web testing links are fixed in the template for `com.dxxrevival.app`. The old `testing_url` config field is ignored and can be removed.

## Deploy from the server checkout

With PowerShell (`pwsh`) installed and `config.local.json` populated, run from the repository root:

```sh
bash server/landing/deploy.sh
```

Optional positional arguments select the config path and web root. The script renders before publishing, atomically replaces only `index.html`, and leaves an unchanged page untouched on reruns. It does not reload nginx or restart matchmaking.

For a server without PowerShell or a checkout, render locally with `pwsh server/landing/build.ps1`, upload only `dist/index.html` and `deploy.sh` to a private staging directory, then run `bash deploy.sh --html /absolute/staging/path/index.html`. An optional final argument selects the web root. This mode does not require PowerShell on the server.

For the initial video update, also run `bash server/install.sh` to apply the checked-in nginx policy allowing `https://www.youtube-nocookie.com` frames and an origin referrer for YouTube. That existing installer reuses saved server settings and certificates, but restarts matchmaking; schedule it accordingly. Subsequent content-only updates need only the landing deploy script. Verify the live page and video playback after deployment.
