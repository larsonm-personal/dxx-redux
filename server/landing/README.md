# Minimal landing page

Copy `config.example.json` to `config.local.json`, fill in the title and links, then run `pwsh server/landing/build.ps1` from the repository root. Deploy only `dist/index.html` to the static web root, never the configuration or source directory.

The local config and generated `dist/` are gitignored. The template and renderer are intended for Git. Keep a private backup of the config. This prevents repository disclosure, **not visitor disclosure**: links in a public page are public, not access-controlled secrets. Google group/testing eligibility remains enforced by Google.

The current VPS web root is `/var/www/dxx-landing`, separate from the ACME challenge root and the fruitfacts website. Serve the landing page at `/`; reserve future `/ws` and `/api/` locations for matchmaking. No player-count requests are made yet.
