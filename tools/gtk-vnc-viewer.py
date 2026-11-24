#!/usr/bin/env python3
"""Minimal VNC viewer using gtk-vnc (independent RFB implementation).

Used to verify Qtopia's Qt/E VNC server interaction, since TigerVNC 1.16.2
has RFB 3.3 compatibility issues with it.

Usage: python3 gtk-vnc-viewer.py [host] [port]
"""
import sys
import gi

gi.require_version("Gtk", "3.0")
gi.require_version("GtkVnc", "2.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gtk, Gdk, GtkVnc, GLib

HOST = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 5900

win = Gtk.Window(title=f"Qtopia VNC - {HOST}:{PORT}")
win.set_default_size(660, 520)
win.connect("destroy", Gtk.main_quit)

vnc = GtkVnc.Display()
win.add(vnc)

def on_error(display, msg):
    print(f"VNC error: {msg}")
    Gtk.main_quit()

def on_disconnected(display):
    print("VNC disconnected")
    Gtk.main_quit()

vnc.connect("vnc-error", on_error)
vnc.connect("vnc-disconnected", on_disconnected)
vnc.set_pointer_grab(True)
vnc.set_keyboard_grab(True)
vnc.realize()
vnc.open_host(HOST, str(PORT))
win.show_all()
Gtk.main()
