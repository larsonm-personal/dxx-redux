# Landing page update

- Use the existing `website_update` branch and leave changes uncommitted for review
- Add the Android port introduction, automatic group approval and red access-delay note, Android/web testing links, and responsive YouTube embed
- Update nginx frame and referrer policies required by the embed
- Add an idempotent landing-only publish script and document the existing installer needed for the nginx update
- Render locally, verify output and unchanged rebuild behavior, and run scoped code quality checks
- Do not deploy during this review pass

## Deployment follow-up

- User subsequently authorized deployment; deployed the rendered page using the existing VPS key
- Added `deploy.sh --html` for servers without PowerShell or a checkout
- Updated only the two nginx video-related headers after verifying the live configuration matched the expected template
- Backed up the page and nginx config in the private server staging directory and reloaded nginx without restarting matchmaking
- Verified HTTPS page bytes and response headers, nginx configuration, and unchanged second deployment
- Browser playback verification unavailable in this session
- Changes remain uncommitted on `website_update`
