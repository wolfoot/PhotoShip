#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
qt_options=()
if [[ -d "$project_dir/.deps/qt/usr/lib/x86_64-linux-gnu" ]]; then
  qt_options+=("-DCMAKE_PREFIX_PATH=$project_dir/.deps/qt/usr")
  export LD_LIBRARY_PATH="$project_dir/.deps/qt/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export QT_PLUGIN_PATH="$project_dir/.deps/qt/usr/lib/x86_64-linux-gnu/qt6/plugins"
fi
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "${qt_options[@]}"
cmake --build build --parallel "${BUILD_JOBS:-4}"
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
mkdir -p dist
cpack --config build/CPackConfig.cmake -B dist
