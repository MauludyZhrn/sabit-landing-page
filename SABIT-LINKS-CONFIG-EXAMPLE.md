# Sabit product links — configuration example

This is a pattern, not a set of live URLs. Replace only with URLs/files that have been verified in the actual project.

## Vite `.env.example`

```dotenv
VITE_SABIT_GITHUB_REPO_URL=
VITE_SABIT_GITHUB_RELEASES_URL=
VITE_SABIT_WINDOWS_INSTALLER_URL=
VITE_SABIT_WINDOWS_INSTALLER_LABEL=Sabit Windows Installer
```

Example values after verification:

```dotenv
VITE_SABIT_GITHUB_REPO_URL=https://github.com/OWNER/REPO
VITE_SABIT_GITHUB_RELEASES_URL=https://github.com/OWNER/REPO/releases/latest
VITE_SABIT_WINDOWS_INSTALLER_URL=/downloads/ACTUAL-INSTALLER-FILENAME.exe
VITE_SABIT_WINDOWS_INSTALLER_LABEL=Sabit v1.0.0 Basic — Windows
```

If the installer is published as a GitHub Release asset, use the exact release asset name:

```dotenv
VITE_SABIT_WINDOWS_INSTALLER_URL=https://github.com/OWNER/REPO/releases/latest/download/ACTUAL-INSTALLER-FILENAME.exe
```

## Implementation rules

- If the current project is not Vite, adapt the environment-variable access to the existing stack.
- Read the values once in a central config module (for example `src/config/productLinks.ts`). Do not hardcode different URLs into multiple components.
- Render each CTA only when its URL is valid. Never use `href="#"` as a temporary destination.
- A local `.exe` must be served from a website path such as `/downloads/file.exe`; a path like `C:\Users\...\file.exe` cannot be used by website visitors.
- For large installer files, prefer GitHub Releases rather than committing the `.exe` to the source repository.
- Confirm that the release asset really exists before using the `/releases/latest/download/...` direct URL.
- Display version, architecture and file size only after verifying the actual file metadata.
