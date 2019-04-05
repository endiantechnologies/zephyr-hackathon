#!/bin/bash

# -- config options, feel free to adjust ------------------------------------- #
DEFAULT_BOARD=nrf52840_pca10056
DEFAULT_PRJ_PATH=samples/hello_world
# ---------------------------------------------------------------------------- #

# Sane bash mode
set -eo pipefail

# Make sure we're inside correct directory, then export environment vars
cd "$(dirname "${BASH_SOURCE[0]}")"
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE=/usr/bin/arm-none-eabi-
export ZEPHYR_BASE=$PWD
. zephyr-env.sh

# Arg parsing
while [[ $# -gt 0 ]]; do
    arg="$1"
    case $arg in
        -b|--board)
            BOARD="$2"
            shift; shift ;;
        -r|--rebuild)
            rebuild_flag=1
            shift ;;
        -f|--flash)
            flash_flag=1
            shift ;;
        -*) echo "Option \"$1\" not understood."
	    exit 1 ;;
         *) test -z "{$proj_path+x}" && {
                  echo "Error: too many args ($arg)"
                  exit 1
              }
            proj_path=$arg
            shift ;;
    esac
done

# Strict bash mode from now on
set -u

CMAKE_FLAGS=${CMAKE_FLAGS:-""}
BOARD=${BOARD:-$DEFAULT_BOARD}
proj_path=${proj_path:-$DEFAULT_PRJ_PATH}

if [ ! -d "$proj_path" ]; then
    echo Project path \""$proj_path"\" does not exist!
    exit 1
fi

echo "Build \"$proj_path\" using board \"$BOARD\""

build_dir="$proj_path/build"
/usr/bin/mkdir -p "$build_dir" && cd "$build_dir"

if [ ! -z ${rebuild_flag+x} ]; then
    echo "Rebuild flag set, removing all files in $build_dir..."
    /usr/bin/rm -rf "${build_dir:?}"/* || {
        # if directory was empty to begin with, do nothing
        true
    }
fi

/usr/bin/cmake -DBOARD="$BOARD" "$CMAKE_FLAGS" ..
/usr/bin/make -j"$(/usr/bin/nproc)"

echo "
-- Zephyr kernel binaries:
    $proj_path/build/zephyr/zephyr.{hex,elf,bin}
"

if [ ! -z ${flash_flag+x} ]; then
    nrfjprog --eraseall
    nrfjprog --program "$build_dir"/zephyr/zephyr.hex
fi
