#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=${STEAMVR_ROOT:-/opt/steamvr}
headers=${OPENVR_HEADERS:-$sdk/tools/hellovr_vulkan_linux/src/openvr/headers}
out=${1:-build/frame-finger}
mkdir -p "$out"
release_flag=
if [ "${FRAME_FINGER_RELEASE:-0}" = 1 ]; then release_flag=-DFRAME_FINGER_RELEASE; fi
g++ $release_flag -std=c++17 -O2 -Wall -Wextra -Werror -pthread -DFRAME_UI_ALL_FINGERS_BETA \
  -I"$headers" src/diagnostics/frame-finger-settings-overlay.cpp \
  $(pkg-config --cflags --libs cairo) -L"$sdk/bin/linuxarm64" -lopenvr_api \
  -o "$out/frame-finger-settings-beta"
runtime_flag=
if [ "${FRAME_FINGER_RUNTIME:-legacy}" = november-beta ]; then
  runtime_flag=-DFRAME_NOV_BETA_CANDIDATE
elif [ "${FRAME_FINGER_RUNTIME:-legacy}" != legacy ]; then
  echo "Unknown runtime profile" >&2
  exit 2
fi
g++ $release_flag $runtime_flag -std=c++17 -O2 -Wall -Wextra -Werror -pthread -fPIC -shared \
  -DFRAME_SCALAR_ALL_FINGERS_BETA -DFRAME_SCALAR_CONTINUOUS_SHADOW \
  -DFRAME_SCALAR_CANDIDATE_SHADOW -DFRAME_SCALAR_OFF_VTABLE_PROBE \
  -DFRAME_SCALAR_STANDALONE_BETA -Wl,-z,nodelete \
  src/diagnostics/frame-scalar-audit-driver.cpp -ldl -o "$out/driver_frame_finger_standalone.so"

if [ -n "$release_flag" ]; then
  g++ -std=c++17 -O2 -Wall -Wextra -Werror -I"$headers" \
    src/diagnostics/frame-release-dashboard.cpp -L"$sdk/bin/linuxarm64" -lopenvr_api \
    -o "$out/frame-release-dashboard"
fi
