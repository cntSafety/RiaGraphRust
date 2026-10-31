"""Updates the version in Cargo.toml to match the version in the main CMakeLists.txt"""

import re
from pathlib import Path

RIAG_RS_ROOT = Path(__file__).parent
SIBLING_RIAG_ROOT = RIAG_RS_ROOT.parent / "riagraph"
RIAG_ROOT = (
    SIBLING_RIAG_ROOT
    if SIBLING_RIAG_ROOT.exists()
    else RIAG_RS_ROOT.parent.parent
)


def get_riag_version():
    cmake_file = RIAG_ROOT / "CMakeLists.txt"
    with open(cmake_file) as f:
        for line in f:
            if line.startswith("project(RiaG VERSION"):
                version = line.split(" ")[2].strip()
                # Make version semver-compatible
                components = version.split(".")
                if len(components) >= 4:
                    version = ".".join(components[0:3]) + "-pre." + ".".join(components[3:])
                return version


if __name__ == "__main__":
    version = get_riag_version()
    version_changed = False
    with open(RIAG_RS_ROOT / "Cargo.toml", encoding="utf-8") as file:
        data = file.readlines()
        section = None
        for index, line in enumerate(data):
            if line.startswith("["):
                section = line.strip().strip("[]")
            if line.startswith("version = ") and section == "package":
                toml_version = re.match('version = "(.*)"', line).group(1)
                if toml_version != version:
                    version_changed = True
                    print(
                        f"Updating version in Cargo.toml from {toml_version} to {version}"
                    )
                    data[index] = re.sub(
                        'version = ".*"', f'version = "{version}"', line
                    )
                    break

    if version_changed:
        with open(RIAG_RS_ROOT / "Cargo.toml", "w", encoding="utf-8") as file:
            file.writelines(data)
