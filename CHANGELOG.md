# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- core: `Renderer` : fix not cleaning up depth texture if non-null (*critical*)
- core : `DebugScene` : fix not setting pipeline handles to nullptr
- multibody/Visualizer : fix not releasing transfer buffers before destroying render context (*critical*)

### Changed

- core : set max number of directional lights to 4

## [0.1.1] - 2025-05-26

### Added

- core/GuiSystem : add overload for adding multiple GUI elements for `DirectionalLight`s (https://github.com/Simple-Robotics/candlewick/pull/69)
- core/CommandBuffer.h : add overload for C-style arrays to uniform-pushing methods (https://github.com/Simple-Robotics/candlewick/pull/69)
- core/Shader : store shader stage (https://github.com/Simple-Robotics/candlewick/pull/69)
- core/DepthAndShadowPass.h : handle multiple shadow maps using a texture atlas (https://github.com/Simple-Robotics/candlewick/pull/69)
- multibody : handle two-light setup w/ shadow mapping (https://github.com/Simple-Robotics/candlewick/pull/69)
- multibody/Visualizer : use `H` key to toggle GUI (https://github.com/Simple-Robotics/candlewick/pull/70)
- shaders : Soft shadows in PBR using percentage-closer filtering (PCF) (https://github.com/Simple-Robotics/candlewick/pull/71)
- core : Tighter shadow frustum around the world-scene AABB (https://github.com/Simple-Robotics/candlewick/pull/71)
- core : add `getAABBCorners()` util function in `Collision.h` header (https://github.com/Simple-Robotics/candlewick/pull/71)

## [0.1.0] - 2025-05-21

### Added

- core : add `guiAddFileDialog` (https://github.com/Simple-Robotics/candlewick/pull/61)
- utils : refactor `VideoRecorder` ctor, add `open()` member function (https://github.com/Simple-Robotics/candlewick/pull/62)
- multibody/Visualizer add screenshot button in visualizer's GUI (https://github.com/Simple-Robotics/candlewick/pull/62)
- utils : add `generateScreenshotFilenameFromTimestamp()` (https://github.com/Simple-Robotics/candlewick/pull/63)
- multibody/Visualizer : GUI : add buttons to start video recording, add `takeScreenshot()` method (https://github.com/Simple-Robotics/candlewick/pull/63)
- utils: add VideoRecorder::close() API (to manually close recorder) (https://github.com/Simple-Robotics/candlewick/pull/61)
- multibody/Visualizer : add `startRecording(std::string_view)` and `stopRecording()` methods, expose to Python (https://github.com/Simple-Robotics/candlewick/pull/65)

### Changed

- utils : avoid public inclusion of `libavutil/pixfmt` header for video recorder (https://github.com/Simple-Robotics/candlewick/pull/61)
- core : `errors.h`: make terminate_with_message a template (with formatting arguments, etc) (https://github.com/Simple-Robotics/candlewick/pull/61)
- multibody/RobotDebug.cpp : fix velocity arrow direction (https://github.com/Simple-Robotics/candlewick/pull/61)
- utils : make `writeTextureToVideoFrame` a member function (https://github.com/Simple-Robotics/candlewick/pull/62)
- utils : rename `writeToFile` to `saveTextureToFile` (https://github.com/Simple-Robotics/candlewick/pull/62)
- utils/PixelFormatConversion.h : make bgra conversion function in-place (https://github.com/Simple-Robotics/candlewick/pull/63)
- utils & third-party : switch from `stb_image_write.h` to `fpng` for writing PNG files (https://github.com/Simple-Robotics/candlewick/pull/64)

### Fixed

- core : fix `CommandBuffer::submitAndAcquireFence()` not setting the internal pointer to null (https://github.com/Simple-Robotics/candlewick/pull/61)
- Fix all calls to `terminate_with_message()` (format string with `%`) (https://github.com/Simple-Robotics/candlewick/pull/62)

## [0.0.7] - 2025-05-17

### Added

- core : make DepthPassInfo an aggregate (https://github.com/Simple-Robotics/candlewick/pull/57)
- core : add `updateTransparencyClassification()` to tag an entity as opaque or untag it (https://github.com/Simple-Robotics/candlewick/pull/57)
- proper support for transparent objects (https://github.com/Simple-Robotics/candlewick/pull/58)
- shaders : add WBOIT composite shader, PBR transparent shader, `utils.glsl` util module, `pbr_lighting.glsl` (https://github.com/Simple-Robotics/candlewick/pull/58)
- core/`CommandBuffer` : type-safe wrappers for pushing uniforms, add `Raw` suffix to raw methods (https://github.com/Simple-Robotics/candlewick/pull/59)

### Changed

- remove `projMatrix` from PBR shader ubo (https://github.com/Simple-Robotics/candlewick/pull/58)
- multibody/RobotScene : early return if pipeline is nullptr (https://github.com/Simple-Robotics/candlewick/pull/58)
- multibody/RobotScene : reorganize pipelines (accomodate for transparent PBR shader)
- shaders : refactor basic PBR shader (move some functions to new `pbr_lighting.glsl`) (https://github.com/Simple-Robotics/candlewick/pull/58)
- interleave debug scene render between robot render system opaque and transparent passes (https://github.com/Simple-Robotics/candlewick/pull/59)
- revamp depth and shadow map pass classes (https://github.com/Simple-Robotics/candlewick/pull/60)
- core/Texture : do not store pointer to `Device` but raw `SDL_GPUDevice *` handle (https://github.com/Simple-Robotics/candlewick/pull/60)

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


[Unreleased]: https://github.com/Simple-Robotics/candlewick/compare/v0.1.1...HEAD
[0.1.1]: https://github.com/Simple-Robotics/candlewick/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/Simple-Robotics/candlewick/compare/v0.0.7...v0.1.0
[0.0.7]: https://github.com/Simple-Robotics/candlewick/compare/v0.0.6...v0.0.7
[0.0.6]: https://github.com/Simple-Robotics/candlewick/compare/v0.0.6...v0.0.6
[0.0.6]: https://github.com/Simple-Robotics/candlewick/compare/v0.0.5...v0.0.6
