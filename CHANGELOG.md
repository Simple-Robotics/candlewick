# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

This is the first release to use a changelog.

### Removed

- Remove `MeshData loadCoalHeightField(const coal::CollisionGeometry &collGeom)`

### Added

- Add template class `strided_view<T>` for non-contiguous evenly-strided data
- Extend support for coal `CollisionGeometry` objects
  - Add support for `coal::GEOM_ELLIPSOID`
  - Add support for `coal::GEOM_CONVEX`
  - Add loader `loadCoalConvex()`

### Changed

- Change signature of `loadCoalPrimitive()` to take `coal::ShapeBase`
- Change signature of `MeshData::getAttribute()` template member function to use strided view



[Unreleased]: https://github.com/Simple-Robotics/candlewick/compare/v0.0.5...HEAD
