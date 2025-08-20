/// \copyright Copyright (c) 2025 INRIA
#pragma once

#include "candlewick/core/errors.h"

#include <pinocchio/multibody/fwd.hpp>
#include <pinocchio/parsers/urdf.hpp>
#include <pinocchio/parsers/srdf.hpp>

#include <vector>
#include <string>
#include <filesystem>
#include <spdlog/fmt/std.h>

namespace candlewick::multibody {
namespace pin = pinocchio;

struct RobotSpec {
  /// Path to the URDF file.
  std::filesystem::path urdf_path;
  /// Path to the SRDF file.
  std::filesystem::path srdf_path;
  /// Path to the base package path.
  std::filesystem::path base_package_path;
  /// Path to the actual model package, relative to the base path.
  std::filesystem::path relative_package_path;
  /// Whether the model should have a pinocchio::JointModelFreeFlyer as its
  /// root.
  bool has_free_flyer = false;

  // Convert URDF and SRDF paths to absolute.
  RobotSpec &ensure_absolute_filepaths() {
    if (base_package_path.is_relative()) {
      terminate_with_message("Field base_package_path must be absolute.");
    }
    if (urdf_path.is_relative()) {
      urdf_path = base_package_path / relative_package_path / urdf_path;
    }
    if (srdf_path.is_relative()) {
      srdf_path = base_package_path / relative_package_path / srdf_path;
    }
    return *this;
  }
};
} // namespace candlewick::multibody

FMT_BEGIN_NAMESPACE
template <> struct formatter<candlewick::multibody::RobotSpec> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const candlewick::multibody::RobotSpec &spec,
              FormatContext &ctx) const {
    return fmt::format_to(ctx.out(),
                          "RobotSpec{{\n"
                          "  urdf_path: \"{}\"\n"
                          "  srdf_path: \"{}\"\n"
                          "  base_package_path: \"{}\"\n"
                          "  relative_package_path: \"{}\"\n"
                          "  has_free_flyer: {}\n"
                          "}}",
                          spec.urdf_path, spec.srdf_path,
                          spec.base_package_path, spec.relative_package_path,
                          spec.has_free_flyer);
  }
};
FMT_END_NAMESPACE

namespace candlewick::multibody {
inline std::vector<std::string> getPackageDirs(const RobotSpec &spec) {
  if (spec.relative_package_path.is_absolute()) {
    terminate_with_message(
        "robot spec relative package path ({:s}) isn't relative.",
        spec.relative_package_path.native());
  }
  if (spec.urdf_path.empty()) {
    terminate_with_message("robot spec's urdf_path field cannot be empty.");
  }
  auto absolute_package_path =
      spec.base_package_path / spec.relative_package_path;
  return {
      spec.base_package_path,       spec.base_package_path.parent_path(),
      absolute_package_path,        absolute_package_path.parent_path(),
      spec.urdf_path.parent_path(),
  };
}

inline pin::Model &loadModel(const RobotSpec &spec, pin::Model &model,
                             bool verbose = false) {
  if (spec.has_free_flyer) {
    pin::urdf::buildModel(spec.urdf_path, pin::JointModelFreeFlyer(), model,
                          verbose);
  } else {
    pin::urdf::buildModel(spec.urdf_path, model, verbose);
  }
  if (std::filesystem::exists(spec.srdf_path)) {
    pin::srdf::loadReferenceConfigurations(model, spec.srdf_path, verbose);
    try {
      pin::srdf::loadRotorParameters(model, spec.srdf_path, verbose);
    } catch (const std::invalid_argument &) {
      // do nothing
    }
  }
  return model;
}

inline void loadModels(const RobotSpec &spec, pin::Model &model,
                       pin::GeometryModel *visual_model,
                       pin::GeometryModel *collision_model,
                       bool verbose = false) {
  loadModel(spec, model, verbose);
  auto package_dirs = getPackageDirs(spec);

  if (visual_model)
    pin::urdf::buildGeom(model, spec.urdf_path, pin::VISUAL, *visual_model,
                         package_dirs);

  if (collision_model) {
    pin::urdf::buildGeom(model, spec.urdf_path, pin::COLLISION,
                         *collision_model, package_dirs);
    if (std::filesystem::exists(spec.srdf_path)) {
      collision_model->addAllCollisionPairs();
      pin::srdf::removeCollisionPairs(model, *collision_model, spec.srdf_path,
                                      false);
    }
  }
}

} // namespace candlewick::multibody
