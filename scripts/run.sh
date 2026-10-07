#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -d "$project_dir/.deps/qt/usr/lib/x86_64-linux-gnu" ]]; then
  export LD_LIBRARY_PATH="$project_dir/.deps/qt/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export QT_PLUGIN_PATH="$project_dir/.deps/qt/usr/lib/x86_64-linux-gnu/qt6/plugins"
fi
exec "$project_dir/build/photoship" "$@"
