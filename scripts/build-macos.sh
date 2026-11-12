#!/bin/bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${1:-$project_dir/build-macos}"
release_dir="${2:-$project_dir/dist-macos}"
version="2.2.0"

cmake -S "$project_dir" -B "$build_dir" -G Xcode \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

cmake --build "$build_dir" --config Release --target \
  NorthstarFX_VST3 NorthstarFX_AU NorthstarFX_Standalone OrbitTest Vst3SignalTest

artifacts="$build_dir/NorthstarFX_artefacts/Release"
stage="$release_dir/pkg-root"
rm -rf "$release_dir"
mkdir -p "$stage/Library/Audio/Plug-Ins/VST3" \
         "$stage/Library/Audio/Plug-Ins/Components" \
         "$release_dir/zip"

cp -R "$artifacts/VST3/Tyap Lyap.vst3" "$stage/Library/Audio/Plug-Ins/VST3/"
cp -R "$artifacts/AU/Tyap Lyap.component" "$stage/Library/Audio/Plug-Ins/Components/"

# Ad-hoc signing keeps every nested Mach-O internally consistent. Distribution
# through Gatekeeper can later replace this with Developer ID signatures.
codesign --force --deep --sign - "$stage/Library/Audio/Plug-Ins/VST3/Tyap Lyap.vst3"
codesign --force --deep --sign - "$stage/Library/Audio/Plug-Ins/Components/Tyap Lyap.component"

"$build_dir/Release/OrbitTest"
for rate in 44100 48000 96000; do
  "$build_dir/Release/Vst3SignalTest" "$stage/Library/Audio/Plug-Ins/VST3/Tyap Lyap.vst3" "$rate"
done
codesign --verify --deep --strict "$stage/Library/Audio/Plug-Ins/VST3/Tyap Lyap.vst3"
codesign --verify --deep --strict "$stage/Library/Audio/Plug-Ins/Components/Tyap Lyap.component"

cp -R "$stage/Library/Audio/Plug-Ins/VST3/Tyap Lyap.vst3" "$release_dir/zip/"
cp -R "$stage/Library/Audio/Plug-Ins/Components/Tyap Lyap.component" "$release_dir/zip/"
cp "$project_dir/INSTALL-MACOS-RU.txt" "$release_dir/zip/INSTALL-RU.txt"

pkgbuild --root "$stage" \
  --identifier audio.northstar.tyaplyap.pkg \
  --version "$version" \
  --install-location / \
  "$release_dir/Tyap-Lyap-$version-macOS-Universal.pkg"

ditto -c -k --sequesterRsrc --keepParent "$release_dir/zip" \
  "$release_dir/Tyap-Lyap-$version-macOS-Universal.zip"

lipo -archs "$stage/Library/Audio/Plug-Ins/VST3/Tyap Lyap.vst3/Contents/MacOS/Tyap Lyap"
lipo -archs "$stage/Library/Audio/Plug-Ins/Components/Tyap Lyap.component/Contents/MacOS/Tyap Lyap"
pkgutil --check-signature "$release_dir/Tyap-Lyap-$version-macOS-Universal.pkg" || true
echo "macOS packages: $release_dir"
