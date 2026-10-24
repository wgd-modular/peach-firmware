#!/usr/bin/env python
#
# Builds the firmwares in src/. Without arguments every firmware is built,
# otherwise only the named ones, e.g. `python build_firmwares.py apricot`.
# `--setup` installs the arduino-cli toolchain, the Seeed RA4M1 core and the
# AGTimer R4 library, `--clean` removes the build output of all firmwares.
#
import argparse
import hashlib
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent
SRC = ROOT / "src"
# Half the cores keeps the machine usable while a firmware compiles.
JOB_COUNT = max(1, (os.cpu_count() or 2) // 2)

# Pinned arduino-cli release, verified by sha256 below, downloaded into the
# project instead of relying on a system install.
ARDUINO_CLI_VERSION = "1.5.1"
ARDUINO_CLI_ASSETS = {
    ("Linux", "x86_64"): (
        "arduino-cli_{v}_Linux_64bit.tar.gz",
        "28a8e119c498a25607821c36cb2dc49e8463941b261a0d99091baa7bc692dd2b",
    ),
    ("Darwin", "x86_64"): (
        "arduino-cli_{v}_macOS_64bit.tar.gz",
        "c982e940027996bea9901050e95fae99c59c1dcfee54beedecaf28141e7bf2e7",
    ),
    ("Darwin", "arm64"): (
        "arduino-cli_{v}_macOS_ARM64.tar.gz",
        "cb952e8c1621c95ef5f1d17831c945e3d0ec5973f89c557a7ec8feb9c4f7d4c9",
    ),
}
ARDUINO_CLI_DIR = ROOT / ".pixi" / "arduino-cli" / ARDUINO_CLI_VERSION
ARDUINO_CLI_BIN = ARDUINO_CLI_DIR / "arduino-cli"
ARDUINO_HOME = ROOT / ".pixi" / "arduino"

# The Seeed Studio core for the XIAO RA4M1.
SEEED_BOARD_MANAGER_URL = (
    "https://files.seeedstudio.com/arduino/package_renesas_1.2.0_index.json"
)
SEEED_CORE = "Seeeduino:renesas_uno"
SEEED_CORE_VERSION = "1.2.0"
ARDUINO_FQBN = "Seeeduino:renesas_uno:XIAO_RA4M1"

# AGTimer R4 is not in the Arduino library index, so it is fetched from
# GitHub at a pinned commit.
AGTIMER_URL = "https://github.com/washiyamagiken/AGTimer_R4_Library"
AGTIMER_COMMIT = "38560b00eacbe7a262f59c389ed71b420709c282"
AGTIMER_DIR = ARDUINO_HOME / "user" / "libraries" / "AGTimer_R4"


def arduino_env():
    env = os.environ.copy()
    env["ARDUINO_DIRECTORIES_DATA"] = str(ARDUINO_HOME / "data")
    env["ARDUINO_DIRECTORIES_DOWNLOADS"] = str(ARDUINO_HOME / "downloads")
    env["ARDUINO_DIRECTORIES_USER"] = str(ARDUINO_HOME / "user")
    return env


def firmwares():
    return sorted(
        folder for folder in SRC.iterdir()
        if folder.is_dir() and (folder / "{}.ino".format(folder.name)).exists()
    )


def download_arduino_cli():
    if ARDUINO_CLI_BIN.exists():
        return 0
    key = (platform.system(), platform.machine())
    if key not in ARDUINO_CLI_ASSETS:
        print("No pinned arduino-cli release for {} {}".format(*key))
        return 1
    name, checksum = ARDUINO_CLI_ASSETS[key]
    name = name.format(v=ARDUINO_CLI_VERSION)
    url = (
        "https://github.com/arduino/arduino-cli/releases/download/"
        "v{v}/{name}".format(v=ARDUINO_CLI_VERSION, name=name)
    )
    print("Downloading: {}".format(url))
    archive = ARDUINO_CLI_DIR / name
    ARDUINO_CLI_DIR.mkdir(parents=True, exist_ok=True)
    urllib.request.urlretrieve(url, archive)

    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != checksum:
        archive.unlink()
        print("Checksum mismatch for {}: expected {}, got {}".format(
            name, checksum, digest))
        return 1

    with tarfile.open(archive) as tar:
        tar.extract("arduino-cli", ARDUINO_CLI_DIR)
    archive.unlink()
    ARDUINO_CLI_BIN.chmod(0o755)
    return 0


def fetch_agtimer():
    if (AGTIMER_DIR / ".git").exists():
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=AGTIMER_DIR,
                              capture_output=True, text=True)
        if head.stdout.strip() == AGTIMER_COMMIT:
            return 0
    print("Fetching: {} at {}".format(AGTIMER_URL, AGTIMER_COMMIT[:7]))
    shutil.rmtree(AGTIMER_DIR, ignore_errors=True)
    AGTIMER_DIR.mkdir(parents=True)
    steps = [
        ["git", "init", "-q"],
        ["git", "fetch", "-q", "--depth", "1", AGTIMER_URL, AGTIMER_COMMIT],
        ["git", "checkout", "-q", "FETCH_HEAD"],
    ]
    for step in steps:
        if subprocess.call(step, cwd=AGTIMER_DIR) != 0:
            return 1
    return 0


def setup():
    if download_arduino_cli() != 0:
        return 1
    env = arduino_env()
    cli = str(ARDUINO_CLI_BIN)
    steps = [
        [cli, "config", "init", "--overwrite",
         "--additional-urls", SEEED_BOARD_MANAGER_URL],
        [cli, "core", "update-index"],
        [cli, "core", "install",
         "{}@{}".format(SEEED_CORE, SEEED_CORE_VERSION)],
    ]
    for step in steps:
        if subprocess.call(step, env=env) != 0:
            return 1
    return fetch_agtimer()


def build_firmware(folder):
    return subprocess.call(
        [str(ARDUINO_CLI_BIN), "compile", "--fqbn", ARDUINO_FQBN,
         "--jobs", str(JOB_COUNT),
         "--output-dir", str(folder / "build"), str(folder)],
        env=arduino_env(),
    )


def clean():
    for folder in firmwares():
        shutil.rmtree(folder / "build", ignore_errors=True)
    return 0


def build(names):
    available = {folder.name: folder for folder in firmwares()}
    unknown = [name for name in names if name not in available]
    if unknown:
        print("Unknown firmware: {}".format(", ".join(unknown)))
        print("Available: {}".format(", ".join(available)))
        return 1

    for name in names or available:
        print("Building: {}".format(name))
        folder = available[name]
        if build_firmware(folder) != 0:
            return 1
        binary = folder / "build" / "{}.ino.bin".format(name)
        print("  -> {}".format(binary.relative_to(ROOT)))
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", nargs="*", help="firmwares to build")
    parser.add_argument("--setup", action="store_true",
                        help="install the arduino-cli toolchain")
    parser.add_argument("--clean", action="store_true",
                        help="remove all build output")
    args = parser.parse_args()
    os.nice(10)
    sys.stdout.reconfigure(line_buffering=True)
    if args.clean:
        sys.exit(clean())
    if args.setup:
        sys.exit(setup())
    sys.exit(build(args.firmware))


if __name__ == '__main__':
    main()
