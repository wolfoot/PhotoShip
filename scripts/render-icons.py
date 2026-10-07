#!/usr/bin/env python3
"""Rebuild PNG/ICO assets from the original SVG (Linux: librsvg, Cairo, Pillow)."""
import ctypes as c
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
rsvg = c.CDLL('librsvg-2.so.2')
cairo = c.CDLL('libcairo.so.2')
gobject = c.CDLL('libgobject-2.0.so.0')
class Rectangle(c.Structure):
    _fields_ = [(field, c.c_double) for field in ('x', 'y', 'width', 'height')]

def signature(library, name, result, *args):
    function = getattr(library, name)
    function.restype, function.argtypes = result, list(args)
    return function

load = signature(rsvg, 'rsvg_handle_new_from_file', c.c_void_p, c.c_char_p, c.c_void_p)
render = signature(rsvg, 'rsvg_handle_render_document', c.c_int, c.c_void_p, c.c_void_p, c.POINTER(Rectangle), c.c_void_p)
surface_create = signature(cairo, 'cairo_image_surface_create', c.c_void_p, c.c_int, c.c_int, c.c_int)
context_create = signature(cairo, 'cairo_create', c.c_void_p, c.c_void_p)
write = signature(cairo, 'cairo_surface_write_to_png', c.c_int, c.c_void_p, c.c_char_p)
destroy_context = signature(cairo, 'cairo_destroy', None, c.c_void_p)
destroy_surface = signature(cairo, 'cairo_surface_destroy', None, c.c_void_p)
unref = signature(gobject, 'g_object_unref', None, c.c_void_p)
handle = load(bytes(root / 'packaging/photoship.svg'), None)
if not handle:
    raise RuntimeError('Cannot load SVG')
try:
    for size in (16, 32, 48, 64, 128, 256, 512):
        surface = surface_create(0, size, size)
        context = context_create(surface)
        try:
            if not render(handle, context, c.byref(Rectangle(0, 0, size, size)), None):
                raise RuntimeError('SVG rendering failed')
            if write(surface, bytes(root / f'packaging/icons/photoship-{size}.png')):
                raise RuntimeError('PNG output failed')
        finally:
            destroy_context(context)
            destroy_surface(surface)
finally:
    unref(handle)
Image.open(root / 'packaging/icons/photoship-256.png').save(
    root / 'packaging/photoship.ico', sizes=[(s, s) for s in (16, 32, 48, 64, 128, 256)])
print('Rebuilt seven PNG sizes and the Windows ICO')
