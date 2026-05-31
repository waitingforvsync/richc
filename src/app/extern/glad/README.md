# glad (vendored, pre-generated)

This is the pre-generated [glad](https://github.com/Dav1dde/glad) OpenGL loader,
vendored as source. It is **not** the glad generator; only its output lives here,
so the build needs no Python and never touches the network.

- Loader: OpenGL 3.3 core, no extensions
- Generated with glad 2.0.8:

```
python -m glad --api gl:core=3.3 --extensions "" --reproducible --out-path . c
```

Layout:

- `include/glad/gl.h` - loader header
- `include/KHR/khrplatform.h` - Khronos platform header
- `src/gl.c` - loader implementation

To regenerate (e.g. to bump the GL version), rerun the command above and replace
these files.
