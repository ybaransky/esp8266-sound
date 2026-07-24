Import("env")
from datetime import datetime

# Regenerate src/build_time.h before every build so the firmware reports the
# actual build time. Because the file's contents change, the source that
# includes it is recompiled each build -- unlike __DATE__/__TIME__, which only
# refresh when that particular translation unit happens to be rebuilt.
header = "src/build_time.h"
content = (
    "#pragma once\n"
    "// Auto-generated before each build by generate_build_time.py. Do not edit.\n"
    '#define BUILD_TIME "%s"\n' % datetime.now().strftime("%Y-%m-%d %H:%M:%S")
)

try:
    with open(header) as existing:
        unchanged = existing.read() == content
except FileNotFoundError:
    unchanged = False

if not unchanged:
    with open(header, "w") as out:
        out.write(content)
