# glad (vendored, pre-generated)

This is the pre-generated [glad](https://github.com/Dav1dde/glad) OpenGL loader,
vendored as source. It is **not** the glad generator; only its output lives here,
so the build needs no Python and never touches the network.

- Loader: OpenGL 3.3 core, plus GL_ARB_clip_control (the gfx backend uses it,
  when the driver exposes it, for native [0,1] reverse-Z depth instead of the
  precision-losing 2z - w shader remap)
- Generated with glad 2.0.8:

```
python -m glad --api gl:core=3.3 --extensions "GL_ARB_clip_control" --reproducible --out-path . c
```

Layout:

- `include/glad/gl.h` - loader header
- `include/KHR/khrplatform.h` - Khronos platform header
- `src/gl.c` - loader implementation

To regenerate (e.g. to bump the GL version), rerun the command above and replace
these files.
