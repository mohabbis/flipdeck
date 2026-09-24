#!/usr/bin/env bash
# Type-checks the Flipper app (src/*.c) against real flipperzero-firmware
# headers, with the firmware's own warning flags, and verifies that every
# external symbol the app needs is exported to apps in api_symbols.csv.
#
# For environments where uFBT can't download its SDK (the normal build is just
# `ufbt` at the repo root, which CI runs). Needs git and arm-none-eabi-gcc.
#
#   scripts/check_flipper_sdk.sh            # uses/creates the firmware cache
#   FLIPPER_FW=/path/to/firmware scripts/check_flipper_sdk.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FW="${FLIPPER_FW:-${XDG_CACHE_HOME:-$HOME/.cache}/flipdeck/flipperzero-firmware}"
REF="${FLIPPER_FW_REF:-dev}"
CC=arm-none-eabi-gcc
NM=arm-none-eabi-nm

command -v "$CC" >/dev/null || { echo "arm-none-eabi-gcc not found" >&2; exit 2; }

if [ ! -f "$FW/targets/f7/api_symbols.csv" ]; then
    echo "Fetching flipperzero-firmware ($REF) into $FW"
    rm -rf "$FW"
    git clone -q --depth 1 --branch "$REF" --filter=blob:none --no-checkout \
        https://github.com/flipperdevices/flipperzero-firmware.git "$FW"
    git -C "$FW" sparse-checkout init --cone
    git -C "$FW" sparse-checkout set furi applications/services targets lib/ble_profile \
        lib/cmsis_core lib/print lib/toolbox lib/drivers site_scons
    git -C "$FW" checkout -q
    # Submodules the SDK headers depend on, at the commits the firmware pins.
    for sub in lib/FreeRTOS-Kernel:https://github.com/FreeRTOS/FreeRTOS-Kernel.git \
               lib/mlib:https://github.com/P-p-H-d/mlib.git \
               lib/stm32wb_copro:https://github.com/flipperdevices/stm32wb_copro.git \
               lib/stm32wb_cmsis:https://github.com/STMicroelectronics/cmsis_device_wb \
               lib/stm32wb_hal:https://github.com/STMicroelectronics/stm32wbxx_hal_driver; do
        path="${sub%%:*}"
        url="${sub#*:}"
        commit="$(git -C "$FW" ls-tree HEAD "$path" | awk '{print $3}')"
        mkdir -p "$FW/$path"
        git -C "$FW/$path" init -q
        git -C "$FW/$path" fetch -q --depth 1 "$url" "$commit"
        git -C "$FW/$path" checkout -q FETCH_HEAD
    done
fi

echo "Firmware: $(git -C "$FW" log -1 --format='%h %cd' --date=short 2>/dev/null || echo unknown)"

INCLUDES=(
    "$FW" "$FW/furi" "$FW/applications/services" "$FW/lib" "$FW/lib/mlib" "$FW/lib/print"
    "$FW/lib/toolbox" "$FW/lib/drivers" "$FW/lib/cmsis_core" "$FW/lib/stm32wb_cmsis/Include"
    "$FW/lib/stm32wb_hal/Inc" "$FW/lib/FreeRTOS-Kernel/include" "$FW/lib/FreeRTOS-Kernel/portable/GCC/ARM_CM4F"
    "$FW/lib/stm32wb_copro/wpan" "$FW/lib/stm32wb_copro/wpan/ble" "$FW/lib/stm32wb_copro/wpan/ble/core"
    "$FW/lib/stm32wb_copro/wpan/interface/patterns/ble_thread" "$FW/lib/stm32wb_copro/wpan/interface/patterns/ble_thread/shci"
    "$FW/lib/stm32wb_copro/wpan/utilities" "$FW/lib/ble_profile"
    "$FW/targets/furi_hal_include" "$FW/targets/f7/ble_glue" "$FW/targets/f7/ble_glue/furi_ble"
    "$FW/targets/f7/ble_glue/services" "$FW/targets/f7/ble_glue/profiles" "$FW/targets/f7/furi_hal"
    "$FW/targets/f7/inc" "$FW/targets/f7/platform_specific" "$FW/targets/f7/fatfs"
    "$ROOT/src"
)
FLAGS=(
    -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mthumb -std=gnu2x -Os
    -fdata-sections -ffunction-sections
    -Wall -Wextra -Werror -Wno-error=deprecated-declarations -Wno-address-of-packed-member
    -Wredundant-decls -Wdouble-promotion -Wundef -Wstrict-prototypes -fsingle-precision-constant
    -D_GNU_SOURCE -DSTM32WB -DSTM32WB55xx -DUSE_FULL_LL_DRIVER -DUSE_FULL_ASSERT -DFURI_NDEBUG
    -DFAP_VERSION='"0.1"'
)
for dir in "${INCLUDES[@]}"; do FLAGS+=("-I$dir"); done

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

# Sources from application.fam, so this checks exactly what uFBT builds.
mapfile -t SOURCES < <(python3 - "$ROOT/application.fam" <<'EOF'
import re, sys
text = open(sys.argv[1]).read()
block = re.search(r"sources\s*=\s*\[(.*?)\]", text, re.S).group(1)
print("\n".join(re.findall(r'"([^"]+)"', block)))
EOF
)

status=0
for source in "${SOURCES[@]}"; do
    if "$CC" "${FLAGS[@]}" -c "$ROOT/$source" -o "$OUT/$(basename "$source").o"; then
        echo "  compiled  $source"
    else
        echo "  FAILED    $source"
        status=1
    fi
done
[ $status -eq 0 ] || exit $status

# Every undefined symbol must be defined by another app object or exported
# to apps ("+") by the firmware API table.
"$NM" --defined-only "$OUT"/*.o | awk '{print $3}' | sort -u > "$OUT/defined"
"$NM" -u "$OUT"/*.o | awk '{print $2}' | sort -u > "$OUT/needed"
awk -F, '($1=="Function" || $1=="Variable") && $2=="+" {print $3}' "$FW/targets/f7/api_symbols.csv" | sort -u > "$OUT/exported"
missing="$(comm -23 "$OUT/needed" "$OUT/defined" | comm -23 - "$OUT/exported" | grep -v '^__aeabi_\|^__stack_chk' || true)"
if [ -n "$missing" ]; then
    echo "Symbols not exported to apps by this firmware:"
    echo "$missing" | sed 's/^/  /'
    exit 1
fi
echo "  $(wc -l < "$OUT/needed") external symbols, all exported by the firmware API"
echo "OK"
