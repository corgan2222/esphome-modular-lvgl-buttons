#!/usr/bin/env python3
"""Capture a screenshot of a running ESPHome SDL window (reusable test helper).

The SDL display runs under XWayland (WSLg, DISPLAY=:0). A root/mss grab returns
black for these windows, so we grab the window drawable directly via Xlib
(get_image ZPixmap, depth 24 = 4 bytes/px, byte order BGRX).

For two-page layouts (square displays: page 1 = creature, page 2 = stats) pass
--pages 2: the script captures page 1, synthesises a fast left->right drag via
XTEST (a "swipe right" -> common/swipe_navigation.yaml's lvgl.page.next), then
captures page 2. The swipe must be FAST (LVGL only fires LV_EVENT_GESTURE above
a minimum velocity), so the drag uses few steps with tiny sleeps.

Usage:
  python3 capture.py --name clawdmeter-sdl --out /path/shot.png
  python3 capture.py --name clawdmeter-sdl --pages 2 --out /path/shot.png
      -> writes /path/shot_page1.png and /path/shot_page2.png
  python3 capture.py --pages 2 --swipe prev ...  # use page.previous instead

Requires (in the esphome venv): python-xlib, pillow.
"""
import argparse
import sys
import time

from Xlib import X, display
from Xlib.ext import xtest
from PIL import Image


def find_window(dpy, root, name):
    """Depth-first walk of the X tree for a window whose WM_NAME == name."""
    stack = [root]
    while stack:
        win = stack.pop()
        try:
            wm = win.get_wm_name()
        except Exception:
            wm = None
        if wm == name:
            return win
        try:
            children = win.query_tree().children
        except Exception:
            children = []
        stack.extend(children)
    return None


def grab(win, w, h):
    """Grab the window drawable as an RGB PIL image."""
    raw = win.get_image(0, 0, w, h, X.ZPixmap, 0xFFFFFFFF)
    return Image.frombytes("RGB", (w, h), raw.data, "raw", "BGRX")


def abs_origin(dpy, win, root):
    geo = win.get_geometry()
    coords = win.translate_coords(root, 0, 0)
    # translate_coords(root,...) gives root-relative offset of root inside win;
    # negate to get window origin in root coords.
    return -coords.x, -coords.y, geo.width, geo.height


def swipe(dpy, ox, oy, w, h, direction):
    """Synthesise a fast horizontal drag and the matching swipe gesture.

    direction "next": left->right drag  -> on_swipe_right -> lvgl.page.next
    direction "prev": right->left drag  -> on_swipe_left  -> lvgl.page.previous

    LVGL only emits LV_EVENT_GESTURE when the drag exceeds a distance AND a
    minimum velocity, so the move is large (10%..90% of width) and fast
    (few steps, ~10ms each).
    """
    y = oy + h // 2
    if direction == "next":
        x0, x1 = ox + int(w * 0.10), ox + int(w * 0.90)
    else:
        x0, x1 = ox + int(w * 0.90), ox + int(w * 0.10)
    xtest.fake_input(dpy, X.MotionNotify, x=x0, y=y)
    dpy.sync(); time.sleep(0.02)
    xtest.fake_input(dpy, X.ButtonPress, 1)
    dpy.sync(); time.sleep(0.02)
    steps = 6
    for i in range(1, steps + 1):
        x = x0 + (x1 - x0) * i // steps
        xtest.fake_input(dpy, X.MotionNotify, x=x, y=y)
        dpy.sync(); time.sleep(0.01)
    xtest.fake_input(dpy, X.ButtonRelease, 1)
    dpy.sync(); time.sleep(0.02)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", default="clawdmeter-sdl")
    ap.add_argument("--out", required=True)
    ap.add_argument("--pages", type=int, default=1)
    ap.add_argument("--swipe", choices=["next", "prev"], default="next",
                    help="page-2 swipe direction (next=page.next, prev=page.previous)")
    ap.add_argument("--settle", type=float, default=2.5,
                    help="seconds to wait for the UI to render before capture")
    args = ap.parse_args()

    dpy = display.Display()
    root = dpy.screen().root

    win = None
    for _ in range(40):  # wait up to ~20s for the window to appear
        win = find_window(dpy, root, args.name)
        if win:
            break
        time.sleep(0.5)
    if not win:
        print(f"ERROR: window '{args.name}' not found", file=sys.stderr)
        return 2

    time.sleep(args.settle)
    ox, oy, w, h = abs_origin(dpy, win, root)

    if args.pages <= 1:
        grab(win, w, h).save(args.out)
        print(f"saved {args.out} ({w}x{h})")
        return 0

    base = args.out[:-4] if args.out.lower().endswith(".png") else args.out
    p1 = f"{base}_page1.png"
    grab(win, w, h).save(p1)
    print(f"saved {p1} ({w}x{h})")

    swipe(dpy, ox, oy, w, h, args.swipe)
    time.sleep(1.0)  # let the page-change animation settle
    p2 = f"{base}_page2.png"
    grab(win, w, h).save(p2)
    print(f"saved {p2} ({w}x{h})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
