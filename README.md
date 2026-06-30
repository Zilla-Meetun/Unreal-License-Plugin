# Unreal License Plugin

Unreal project containing the `LicenseSystem` plugin. The plugin checks a configured license key against a license server before allowing editor and runtime use.

## Local Test Server

The plugin defaults to a localhost test endpoint:

```text
http://127.0.0.1:8765/licenses
```

Before opening the project with the default settings, start the included Python test server:

```powershell
python ".\Tools\license_test_server.py"
```

The default valid test license is:

```text
11111111-1111-1111-1111-111111111111
```

To use different valid test licenses:

```powershell
$env:VALID_LICENSES="aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa,bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb"
python ".\Tools\license_test_server.py"
```

The server responds to:

```text
GET /licenses/<license_uuid>
```

With JSON shaped like:

```json
{
  "license": {
    "id": "11111111-1111-1111-1111-111111111111",
    "is_valid": true
  }
}
```

## Editing License Settings

In Unreal Editor, open:

```text
Project Settings > Plugins > Authenticator
```

Editable fields:

- `License`: the license UUID to validate.
- `License Server URL`: the base endpoint used for validation.
- `License Server Authorization Header`: optional full Authorization header value.

By default, the URL is provided by C++ as:

```text
http://127.0.0.1:8765/licenses
```

Unreal only writes values to `Config/DefaultGame.ini` after you change/save them in Project Settings. Saved values appear under:

```ini
[/Script/LicenseSystem.AuthenticatorSettings]
License=11111111-1111-1111-1111-111111111111
LicenseServerUrl=http://127.0.0.1:8765/licenses
LicenseServerAuthorizationHeader=
```

## Before Packaging

Before packaging a real project:

1. Replace `License Server URL` with your production endpoint.
2. Replace the mock `License` with a real license key.
3. Set `License Server Authorization Header` only if your server requires it.
4. Make sure the packaged build can reach the configured endpoint.
5. Do not ship with the localhost URL unless the license server will run locally for end users.

The plugin appends the license key to the configured URL unless the URL contains `{License}`.

Examples:

```text
https://example.com/licenses
```

becomes:

```text
https://example.com/licenses/<license_uuid>
```

And:

```text
https://example.com/validate?key={License}
```

becomes:

```text
https://example.com/validate?key=<license_uuid>
```
