import subprocess
from datetime import datetime

from SCons.Script import Import

Import("env")


def _git_describe():
    try:
        out = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip()
    except Exception:
        return None


def _build_version():
    git_tag = _git_describe()
    if git_tag:
        return git_tag
    return datetime.utcnow().strftime("%Y%m%d%H%M%S")


env_name = env.subst("$PIOENV") or "unknown"
version = _build_version()
full_version = f"{env_name}-{version}"

env.Append(CPPDEFINES=[f'FW_VERSION=\\"{full_version}\\"'])

