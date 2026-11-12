"""Create a private local .env from the committed example on first run."""

from __future__ import annotations

import secrets
from pathlib import Path


ROOT = Path(__file__).resolve().parent
ENV_FILE = ROOT / ".env"
EXAMPLE_FILE = ROOT / ".env.example"


def main() -> None:
    values = {
        "JWT_SECRET": secrets.token_urlsafe(48),
        "ADMIN_PASSWORD": secrets.token_urlsafe(16),
        "VIEWER_PASSWORD": secrets.token_urlsafe(16),
        "WEBHOOK_TOKEN": secrets.token_urlsafe(32),
    }
    source = ENV_FILE if ENV_FILE.exists() else EXAMPLE_FILE
    lines = source.read_text(encoding="utf-8").splitlines()
    output = []
    found: set[str] = set()
    generated = False
    for line in lines:
        key = line.split("=", 1)[0] if "=" in line else ""
        if key in values:
            found.add(key)
            value = line.split("=", 1)[1]
            if not value or value.startswith("replace_with_"):
                line = f"{key}={values[key]}"
                generated = True
        output.append(line)
    for key, value in values.items():
        if key not in found:
            output.append(f"{key}={value}")
            generated = True
    if not generated:
        return
    ENV_FILE.write_text("\n".join(output) + "\n", encoding="utf-8")
    print("Created gateway/.env with random local credentials.")
    print("Open gateway/.env to see the admin/viewer passwords.")


if __name__ == "__main__":
    main()
