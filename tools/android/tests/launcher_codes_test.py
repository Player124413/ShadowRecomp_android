#!/usr/bin/env python3
"""Checks the launcher's list of built-in codes against the game's.

The mod manager of the launcher (ModManagerActivity.java) offers the hidden boolean values
of the [Codes] section of ShadowRecomp/user/config_def.h by name. The game ignores a name
it does not know, so a code renamed upstream would silently stop working, and a new one
would not be offered. An update from upstream is what changes that list.
"""
import re
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[3]
LAUNCHER = REPOSITORY / 'android-apk' / 'app' / 'src' / 'main' / 'java' / 'org' / 'shadowrecomp' / 'android' / 'ModManagerActivity.java'
CONFIG = REPOSITORY / 'ShadowRecomp' / 'user' / 'config_def.h'

failures = 0


def check(condition, what):
    global failures
    if not condition:
        failures += 1
        print(f'FAIL {what}')


java = LAUNCHER.read_text(encoding='utf-8')
start = java.find('CODE_DEFS = {')
check(start >= 0, f'{LAUNCHER.name} has the CODE_DEFS table')
end = java.find('};', start)
offered = re.findall(r'\{\s*"(\w+)"\s*,\s*"[^"]+"\s*\}', java[start:end]) if start >= 0 else []

known = re.findall(r'^CONFIG_DEFINE_HIDDEN\("Codes",\s*bool,\s*(\w+),', CONFIG.read_text(encoding='utf-8'), re.MULTILINE)

check(len(known) >= 20, f'{CONFIG.name} lists the codes ({len(known)} found)')
check(len(offered) == len(set(offered)), 'no code is offered twice')

for name in sorted(set(offered) - set(known)):
    check(False, f'the launcher offers "{name}", which the game does not know (renamed or removed in config_def.h?)')

for name in sorted(set(known) - set(offered)):
    check(False, f'the game knows the code "{name}", which the launcher does not offer (add it to CODE_DEFS)')

print(f'{len(offered)} codes offered, {len(known)} known to the game, {failures} failed')
sys.exit(1 if failures else 0)
