#! /bin/bash -e
# This script builds the mbedTLS submodule.
# It's run by Xcode when building the target LiteCoreWebSocket.

source "$SRCROOT/build_setup.sh" mbedtls

# Set up the CMake build options:
CMAKE_OPTS="$CMAKE_OPTS \
            -DCMAKE_BUILD_TYPE=RelWithDebugInfo \
            -DENABLE_PROGRAMS=0 \
            -DENABLE_TESTING=0"

echo "CMake options: $CMAKE_OPTS"

# Build!
cmake "$SRCROOT/../vendor/mbedtls" $CMAKE_OPTS
make

# Copy the resulting static libraries to the Xcode build dir where the linker will find them:
mkdir -p "$TARGET_BUILD_DIR"
cp library/libmbed*.a "$TARGET_BUILD_DIR/"
