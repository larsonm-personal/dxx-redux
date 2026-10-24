# Landing page update

- Use the existing `website_update` branch and leave changes uncommitted for review
- Add the Android port introduction, automatic group approval and red access-delay note, Android/web testing links, and responsive YouTube embed
- Update nginx frame and referrer policies required by the embed
- Add an idempotent landing-only publish script and document the existing installer needed for the nginx update
- Render locally, verify output and unchanged rebuild behavior, and run scoped code quality checks
- Do not deploy during this review pass
