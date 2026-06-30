#!/usr/bin/env python3
"""Local test license server for the Unreal LicenseSystem plugin."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
import re
from urllib.parse import unquote, urlparse


HOST = os.environ.get("LICENSE_TEST_HOST", "127.0.0.1")
PORT = int(os.environ.get("LICENSE_TEST_PORT", "8765"))
VALID_LICENSES = {
    value.strip().lower()
    for value in os.environ.get(
        "VALID_LICENSES",
        "11111111-1111-1111-1111-111111111111",
    ).split(",")
    if value.strip()
}
UUID_PATTERN = re.compile(
    r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
)


class LicenseHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        parts = [unquote(part) for part in parsed.path.split("/") if part]

        if len(parts) != 2 or parts[0] != "licenses":
            self.send_json(404, {"error": "Expected /licenses/<license_uuid>"})
            return

        license_key = parts[1]
        is_uuid = bool(UUID_PATTERN.match(license_key))
        is_valid = is_uuid and license_key.lower() in VALID_LICENSES

        self.send_json(
            200,
            {
                "license": {
                    "id": license_key,
                    "is_valid": is_valid,
                }
            },
        )

    def send_json(self, status_code, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        print("%s - %s" % (self.address_string(), fmt % args))


if __name__ == "__main__":
    server = ThreadingHTTPServer((HOST, PORT), LicenseHandler)
    print(f"License test server listening on http://{HOST}:{PORT}/licenses/<license_uuid>")
    print("Default valid license: 11111111-1111-1111-1111-111111111111")
    print("Override valid licenses with: set VALID_LICENSES=uuid1,uuid2")
    server.serve_forever()
