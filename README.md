> **This project is obsolete since all its developments have been integrated into [tsdViewer](https://github.com/NVIDIA/VisRTX/tree/next_release/tsd/apps/interactive/viewer).**

# ANARI Mini-Viewer for Volumetric Data

Mini viewer application able to render volumes with ANARI. Dependencies are

- ANARI-SDK: https://github.com/KhronosGroup/ANARI-SDK/ (version 0.8.x, with
`INSTALL_VIEWER_LIBRARY=ON`)
- VTK (optional, support for unstructured volume files)
- HDF5 (optional, support for FLASH AMR)

## Usage:

```
anariVolumeViewer [{--help|-h}]
   [{--verbose|-v}] [{--debug|-g}]
   [{--library|-l} <ANARI library>]
   [{--trace|-t} <directory>]
   [{--dims|-d} <dimx dimy dimz>]
   [{--type|-t} [{uint8|uint16|float32}]
```

## Volume files this was tested with:

Structured-regular volumes (RAW format):
- https://klacansky.com/open-scivis-datasets/

In case of RAW volumes, the app tries to guess the correcct input dimensions
and data format from the file name (can be overwritten via cmdline args)

AMR volumes (FLASH format):
- http://silcc.mpa-garching.mpg.de


Unstructured volumes:
- As exported from ParaView, data is obtained from the first field, which is
  required to be a scalar of type float or double

AMR and Unstructured volumes/spatial fields are realized as ANARI extensions,
roughly follow the input format of OSPRay

## License

Apache 2 (if not noted otherwise)
