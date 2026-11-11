#!/usr/bin/env python3

from pathlib import Path
import sys
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


def main() -> None:
    repo_dir = Path(sys.argv[1]).resolve()
    archive = repo_dir / "dist" / "design-slop.skill"
    sources = (
        ("design-slop/SKILL.md", repo_dir / "SKILL.md"),
        (
            "design-slop/references/before-after-examples.md",
            repo_dir / "references" / "before-after-examples.md",
        ),
        (
            "design-slop/references/slop-patterns.md",
            repo_dir / "references" / "slop-patterns.md",
        ),
    )

    archive.parent.mkdir(parents=True, exist_ok=True)
    archive.unlink(missing_ok=True)

    with ZipFile(archive, "w", compression=ZIP_DEFLATED, compresslevel=9) as bundle:
        for archive_name, source in sources:
            info = ZipInfo(archive_name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            bundle.writestr(info, source.read_bytes(), compresslevel=9)

    print(f"Built {archive}")


if __name__ == "__main__":
    main()
