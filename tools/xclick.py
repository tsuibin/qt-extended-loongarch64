#!/usr/bin/env python3
"""Inject a mouse click at absolute X screen coordinates via XTEST.

Usage: python3 xclick.py <x> <y> [button]
"""
import ctypes
import sys

x = int(sys.argv[1]) if len(sys.argv) > 1 else 960
y = int(sys.argv[2]) if len(sys.argv) > 2 else 590
button = int(sys.argv[3]) if len(sys.argv) > 3 else 1

x11 = ctypes.CDLL("libX11.so.6")
xtst = ctypes.CDLL("libXtst.so.6")

x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
x11.XFlush.argtypes = [ctypes.c_void_p]
x11.XCloseDisplay.argtypes = [ctypes.c_void_p]

d = x11.XOpenDisplay(None)
if not d:
    print("cannot open display")
    sys.exit(1)
# move pointer to (x,y) on screen 0
xtst.XTestFakeMotionEvent(d, 0, x, y, 0)
x11.XFlush(d)
# click
xtst.XTestFakeButtonEvent(d, button, 1, 0)  # press
x11.XFlush(d)
xtst.XTestFakeButtonEvent(d, button, 0, 0)  # release
x11.XFlush(d)
x11.XCloseDisplay(d)
print(f"clicked at {x},{y} button {button}")
