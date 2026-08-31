"""Keep LDF off lvgl examples/demos/tests and unused backends.

lvgl's library.json only sets includeDir=. PlatformIO then treats src/ as the
compile root, but still walks GPU/OS/PC drivers that this firmware never uses.
This runs as a pre: script so library.json is patched before LDF.
"""

import json
import os

Import("env")

# Relative to the lvgl package root (srcDir = ".").
_LVGL_SRC_FILTER = [
    "+<src/>",
    "-<examples/>",
    "-<demos/>",
    "-<tests/>",
    "-<docs/>",
    "-<scripts/>",
    "-<env_support/>",
    "-<src/drivers/>",
    "-<src/draw/dma2d/>",
    "-<src/draw/espressif/>",
    "-<src/draw/eve/>",
    "-<src/draw/nanovg/>",
    "-<src/draw/nema_gfx/>",
    "-<src/draw/nxp/>",
    "-<src/draw/opengles/>",
    "-<src/draw/renesas/>",
    "-<src/draw/sdl/>",
    "-<src/draw/vg_lite/>",
    "-<src/draw/snapshot/>",
    "-<src/libs/>",
    "+<src/libs/gif/>",
    "+<src/libs/bin_decoder/>",
    "-<src/debugging/monkey/>",
    "-<src/debugging/test/>",
    "-<src/debugging/vg_lite_tvg/>",
    "-<src/widgets/lottie/>",
    "-<src/widgets/3dtexture/>",
    "-<src/widgets/ime/>",
    "-<src/others/file_explorer/>",
    "-<src/others/fragment/>",
    "-<src/others/translation/>",
    "-<src/osal/>",
    "+<src/osal/lv_os.c>",
    "+<src/osal/lv_os.h>",
    "+<src/osal/lv_os_none.c>",
    "+<src/osal/lv_os_none.h>",
    "+<src/osal/lv_os_private.h>",
    "-<src/stdlib/micropython/>",
    "-<src/stdlib/rtthread/>",
    "-<src/stdlib/uefi/>",
]

_LVGL_BUILD = {
    "includeDir": ".",
    "srcDir": ".",
    "srcFilter": _LVGL_SRC_FILTER,
}


def _patch_lvgl(pkg_dir):
    libjson = os.path.join(pkg_dir, "library.json")
    if not os.path.isfile(libjson):
        return False
    try:
        with open(libjson, encoding="utf-8") as fp:
            data = json.load(fp)
        if data.get("build") == _LVGL_BUILD:
            return False
        data["build"] = dict(_LVGL_BUILD)
        with open(libjson, "w", encoding="utf-8") as fp:
            json.dump(data, fp, indent="\t")
            fp.write("\n")
    except OSError as exc:
        print("LDF: could not patch %s (%s)" % (libjson, exc))
        return False
    print("LDF: patched %s srcFilter (skip examples/demos/unused backends)" % pkg_dir)
    return True


def _lvgl_dirs():
    # Only patch PlatformIO's copy under .pio/libdeps, never the Arduino sketchbook.
    dirs = []
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
    pioenv = env.get("PIOENV", "")
    if libdeps and pioenv:
        dirs.append(os.path.join(libdeps, pioenv, "lvgl"))
    return dirs


for _path in _lvgl_dirs():
    _patch_lvgl(_path)
