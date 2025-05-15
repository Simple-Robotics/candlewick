# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- core : make DepthPassInfo an aggregate
- core : add `updateTransparencyClassification()` to tag an entity as opaque or untag it

## [0.0.6] - 2025-05-14

This is the first release to use a changelog.

### Removed

- Remove `MeshData loadCoalHeightField(const coal::CollisionGeometry &collGeom)`

### Added

- `RobotScene.h`: move invalid_enum() out to public header
- Add template class `strided_view<T>` for non-contiguous evenly-strided data
- `errors.h`: Use `string_view` for `_error_message_impl`
- Extend support for coal `CollisionGeometry` objects
  - Add support for `coal::GEOM_ELLIPSOID`
  - Add support for `coal::GEOM_CONVEX`
  - Add loader `loadCoalConvex()`

### Changed

- multibody : change signature and rename { guiPinocchioModelInfo() => guiAddPinocchioModelInfo() } ([#54](https://github.com/Simple-Robotics/candlewick/pull/54))
- core : pass wrap width to showCandlewickAboutWindow ([#54](https://github.com/Simple-Robotics/candlewick/pull/54))
- Visualizer: set next window pos ([#54](https://github.com/Simple-Robotics/candlewick/pull/54))
- core/GuiSystem.h : rename some free functions, group them in topic
- Read geometry object `meshColor` `meshScale` when updating robot scene ([#50](https://github.com/Simple-Robotics/candlewick/pull/50))
- Set minimum version of ffmpeg to 7.x ([#50](https://github.com/Simple-Robotics/candlewick/pull/50))
- Change signature of `loadCoalPrimitive()` to take `coal::ShapeBase`
- Change signature of `MeshData::getAttribute()` template member function to use strided view

### Fixed

- Visual bug (uninitialized data) when disabling SSAO


[Unreleased]: https://github.com/Simple-Robotics/candlewick/compare/v0.0.6...HEAD
[0.0.6]: https://github.com/Simple-Robotics/candlewick/compare/v0.0.6...v0.0.6
[0.0.6]: https://github.com/Simple-Robotics/candlewick/compare/v0.0.5...v0.0.6
