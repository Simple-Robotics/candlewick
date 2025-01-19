#include "RobotScene.h"
#include "LoadPinocchioGeometry.h"
#include "../core/errors.h"
#include "../core/Shader.h"
#include "../core/TransformUniforms.h"
#include "../utils/CameraControl.h"
#include "../third-party/magic_enum.hpp"

#include <coal/BVH/BVH_model.h>
#include <pinocchio/multibody/data.hpp>

namespace candlewick::multibody {
RobotScene::PipelineType
RobotScene::pinGeomToPipeline(const coal::CollisionGeometry &geom) {
  using enum coal::OBJECT_TYPE;
  const auto objType = geom.getObjectType();
  switch (objType) {
  case OT_GEOM:
    return PIPELINE_TRIANGLEMESH;
  case OT_HFIELD:
    return PIPELINE_HEIGHTFIELD;
  case OT_BVH: {
    using enum coal::BVHModelType;
    // cast to BVHModelBase, check if mesh or points
    const auto &bvh = dynamic_cast<const coal::BVHModelBase &>(geom);
    switch (bvh.getModelType()) {
    case BVH_MODEL_POINTCLOUD:
      return PIPELINE_POINTCLOUD;
    case BVH_MODEL_TRIANGLES:
      return PIPELINE_TRIANGLEMESH;
    case BVH_MODEL_UNKNOWN:
      throw InvalidArgument("Unknown BVH model type.");
    }
  }
  case OT_COUNT:
  case OT_OCTREE:
  case OT_UNKNOWN:
    char errbuf[32];
    SDL_snprintf(errbuf, sizeof(errbuf), "Unsupported object type %s",
                 magic_enum::enum_name(objType).data());
    throw InvalidArgument(errbuf);
  }
}

auto RobotScene::addEnvironmentObject(MeshData &&data, Mat4f placement,
                                      PipelineType pipe_type)
    -> EnvironmentObject & {
  Mesh mesh = createMesh(_device, data);
  uploadMeshToDevice(_device, mesh.toView(), data);
  return environmentObjects.emplace_back(
      true, std::move(mesh), std::move(data.material), placement, pipe_type);
}

RobotScene::RobotScene(const Renderer &renderer,
                       const pin::GeometryModel &geom_model,
                       const pin::GeometryData &geom_data, Config config)
    : _device(renderer.device), _geomData(geom_data) {
  for (const auto type : {
           PIPELINE_TRIANGLEMESH, PIPELINE_HEIGHTFIELD,
           //  PIPELINE_POINTCLOUD
       }) {
    if (!config.pipeline_configs.contains(type)) {
      char errbuf[64];
      size_t maxlen = sizeof(errbuf);
      SDL_snprintf(errbuf, maxlen,
                   "Missing pipeline config for pipeline type %s",
                   magic_enum::enum_name(type).data());
      throw InvalidArgument(errbuf);
    }
  }

  auto &dev = renderer.device;
  auto swapchain_format = renderer.getSwapchainTextureFormat();

  for (pin::GeomIndex geom_id = 0; geom_id < geom_model.ngeoms; geom_id++) {
    const auto &geom_obj = geom_model.geometryObjects[geom_id];
    auto meshDatas = loadGeometryObject(geom_obj);
    PipelineType pipeline_type = pinGeomToPipeline(*geom_obj.geometry);
    auto [mesh, views] = createMeshFromBatch(dev, meshDatas, true);
    assert(validateMesh(mesh));
    const auto &layout = mesh.layout;
    robotObjects.push_back({
        geom_id,
        std::move(mesh),
        std::move(views),
        extractMaterials(meshDatas),
        pipeline_type,
    });
    if (!renderPipelines.contains(pipeline_type)) {
      auto pipe_config = config.pipeline_configs.at(pipeline_type);
      SDL_Log("%s(): building pipeline for type %s", __FUNCTION__,
              magic_enum::enum_name(pipeline_type).data());
      auto *pipeline =
          createPipeline(dev, layout, swapchain_format, renderer.depth_format,
                         pipeline_type, pipe_config);
      assert(pipeline);
      renderPipelines.emplace(pipeline_type, PipelineData{pipeline, layout});
    }
  }
}

void RobotScene::render(Renderer &renderer, const Camera &camera) {
  SDL_GPUColorTargetInfo color_target{
      .texture = renderer.swapchain,
      .clear_color{},
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE,
  };
  SDL_GPUDepthStencilTargetInfo depth_target{
      .texture = renderer.depth_texture,
      .clear_depth = 1.0f,
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE,
      .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
      .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
  };

  struct alignas(16) LightUniform {
    DirectionalLight a;
    GpuVec3 viewPos;
  };
  const LightUniform lightUbo{directionalLight, camera.position()};
  // view-projection matrix P * V
  const Mat4f viewProj = camera.viewProj();
  SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(
      renderer.command_buffer, &color_target, 1, &depth_target);
  renderer.pushFragmentUniform(FragmentUniformSlots::LIGHTING, &lightUbo,
                               sizeof(lightUbo));

  // iterate over primitive types in the keys
  for (const auto &[pipeline_type, pipe_data] : renderPipelines) {
    SDL_BindGPUGraphicsPipeline(render_pass, pipe_data.pipeline);
    for (const auto &obj : robotObjects) {
      pin::GeomIndex geom_id = obj.geom_index;
      if (obj.pipeline_type != pipeline_type)
        continue;
      const auto &mesh = obj.mesh;
      const auto &placement = _geomData.oMg[geom_id].cast<float>();
      const Mat4f modelMat = placement.toHomogeneousMatrix();
      const Mat3f normalMatrix =
          modelMat.topLeftCorner<3, 3>().inverse().transpose();
      const Mat4f mvp = viewProj * modelMat;
      TransformUniformData data{
          .model = modelMat,
          .mvp = mvp,
          .normalMatrix = normalMatrix,
      };
      renderer.pushVertexUniform(VertexUniformSlots::TRANSFORM, &data,
                                 sizeof(data));
      renderer.bindMesh(render_pass, mesh);
      // loop over views and materials
      for (size_t j = 0; j < obj.views.size(); j++) {
        auto material = obj.materials[j].toUniform();
        renderer.pushFragmentUniform(FragmentUniformSlots::MATERIAL, &material,
                                     sizeof(material));
        renderer.drawView(render_pass, obj.views[j]);
      }
    }

    for (const auto &[status, mesh, _material, modelMat, object_pipeline_type] :
         environmentObjects) {
      if (!status || (object_pipeline_type != pipeline_type))
        continue;

      auto material = _material.toUniform();
      const Mat3f normalMatrix =
          modelMat.topLeftCorner<3, 3>().inverse().transpose();
      const Mat4f mvp = viewProj * modelMat;
      TransformUniformData data{
          .model = modelMat,
          .mvp = mvp,
          .normalMatrix = normalMatrix,
      };
      renderer.pushVertexUniform(VertexUniformSlots::TRANSFORM, &data,
                                 sizeof(data));
      renderer.bindMesh(render_pass, mesh);
      renderer.pushFragmentUniform(FragmentUniformSlots::MATERIAL, &material,
                                   sizeof(material));
      renderer.draw(render_pass, mesh);
    }

    SDL_EndGPURenderPass(render_pass);
  }
}

void RobotScene::release() {
  for (auto &obj : robotObjects) {
    obj.mesh.release(_device);
  }
  for (auto &obj : environmentObjects) {
    obj.mesh.release(_device);
  }
  if (!_device)
    return;
  robotObjects.clear();
  environmentObjects.clear();

  for (auto &[primType, pipe_data] : renderPipelines) {
    SDL_ReleaseGPUGraphicsPipeline(_device, pipe_data.pipeline);
    pipe_data.pipeline = nullptr;
  }
}

SDL_GPUGraphicsPipeline *
RobotScene::createPipeline(const Device &dev, const MeshLayout &layout,
                           SDL_GPUTextureFormat render_target_format,
                           SDL_GPUTextureFormat depth_stencil_format,
                           PipelineType type,
                           const Config::PipelineConfig &config) {

  Shader vertex_shader{dev, config.vertex_shader_path,
                       config.num_vertex_uniforms};
  Shader fragment_shader{dev, config.fragment_shader_path,
                         config.num_frag_uniforms};

  SDL_GPUColorTargetDescription color_target;
  SDL_zero(color_target);
  color_target.format = render_target_format;
  SDL_GPUGraphicsPipelineCreateInfo desc{
      .vertex_shader = vertex_shader,
      .fragment_shader = fragment_shader,
      .vertex_input_state = layout.toVertexInputState(),
      .primitive_type = getPrimitiveTopologyForType(type),
      .depth_stencil_state{
          .compare_op = config.depth_compare_op,
          .enable_depth_test = true,
          .enable_depth_write = true,
      },
      .target_info{
          .color_target_descriptions = &color_target,
          .num_color_targets = 1,
          .depth_stencil_format = depth_stencil_format,
          .has_depth_stencil_target = true,
      },
  };
  desc.rasterizer_state.cull_mode = config.cull_mode;
  desc.rasterizer_state.fill_mode = config.fill_mode;
  return SDL_CreateGPUGraphicsPipeline(dev, &desc);
}

///// ROBOT DEBUG MODULE

// RobotDebugModule::RobotDebugModule(const pin::Data &data) : data(data) {}

// void RobotDebugModule::addDrawCommands(DebugScene &scene,
//                                        const Camera &camera) {
//   const auto &viewMat = camera.viewMatrix();
//   const Mat4f projView = camera.projection * viewMat;

//   for (pin::FrameIndex i = 0; i < data.oMf.size(); i++) {
//     const auto pose = data.oMf[i].cast<float>();
//     const Mat4f M = pose.toHomogeneousMatrix();
//     const Mat3f N = M.topLeftCorner<3, 3>().inverse().transpose();
//     for (int axis = 0; axis < 3; axis++) {
//       scene.draw_commands.push_back(
//           {.shape = &triad[axis],
//            .transform = {.model = M, .mvp = projView * M, .normalMatrix =
//            N}});
//     }
//   }
// }

} // namespace candlewick::multibody
