Bundled third-party dependencies for the MinGW build.

The `msys2-mingw64` directory contains files extracted from official MSYS2
MinGW64 packages:

- `mingw-w64-x86_64-gmp-6.3.0-2-any.pkg.tar.zst`
  - SHA256: `8924433974c4add46cb46ea4f6ef283b5c5139d3f552375115b5580f855015cc`
- `mingw-w64-x86_64-mpfr-4.2.2-3-any.pkg.tar.zst`
  - SHA256: `9ecbc05f1f855bc656a8f111d367f61fbd90dbbfcf469ba74d6d5dd1ec07a542`

The packages are used so CLion's bundled MinGW toolchain can configure, link,
and run the calculator without requiring a global MSYS2, vcpkg, or Conan setup.
