#include "candlewick/multibody/Multibody.h"
#include "candlewick/multibody/Visualizer.h"
#include "Messages.h"

#include <pinocchio/serialization/model.hpp>
#include <pinocchio/serialization/geometry.hpp>
#include <pinocchio/algorithm/kinematics.hpp>

#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <msgpack.hpp>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

#include <spdlog/cfg/env.h>

namespace cdw = candlewick;
namespace pin = pinocchio;
using namespace cdw::runtime;
using cdw::multibody::Visualizer;

#define CANDLEWICK_RUNTIME_DEFINE_COMMAND(name)                                \
  constexpr std::string_view CMD_##name = #name

CANDLEWICK_RUNTIME_DEFINE_COMMAND(send_models);
CANDLEWICK_RUNTIME_DEFINE_COMMAND(state_update);
CANDLEWICK_RUNTIME_DEFINE_COMMAND(send_cam_pose);
CANDLEWICK_RUNTIME_DEFINE_COMMAND(reset_camera);
CANDLEWICK_RUNTIME_DEFINE_COMMAND(start_recording);
CANDLEWICK_RUNTIME_DEFINE_COMMAND(stop_recording);
CANDLEWICK_RUNTIME_DEFINE_COMMAND(clean);
CANDLEWICK_RUNTIME_DEFINE_COMMAND(toggle_gui);

using RowMat4d = Eigen::Matrix<pin::context::Scalar, 4, 4, Eigen::RowMajor>;

struct ApplicationContext {
  pin::Model model;
  pin::GeometryModel visual_model;
  pin::GeometryModel collision_model;
  zmq::context_t ctx{};
  zmq::socket_t sync_sock{ctx, zmq::socket_type::rep};
  zmq::socket_t state_sock{ctx, zmq::socket_type::sub};
};

/// Handle first incoming message.
bool handle_first_message(ApplicationContext &app_ctx) {
  zmq::socket_t &sock = app_ctx.sync_sock;

  while (true) {
    std::array<zmq::message_t, 2> msgs;

    auto ret = zmq::recv_multipart_n(sock, msgs.begin(), 2);
    if (!ret)
      return false;

    auto msg_header = msgs[0].to_string_view();
    if (msg_header == CMD_send_models) {
      msgpack::object_handle oh = get_handle_from_zmq_msg(std::move(msgs[1]));
      auto obj = oh.get();
      std::array<std::string, 2> model_strings;
      obj.convert(model_strings);
      app_ctx.model.loadFromString(model_strings[0]);
      app_ctx.visual_model.loadFromString(model_strings[1]);
      // send response only when models loaded.
      sock.send(zmq::str_buffer("ok"));

      spdlog::info("Loaded model with {:d} joints", app_ctx.model.njoints);
      spdlog::info("Loaded geometry model with {:d} gobjs",
                   app_ctx.visual_model.ngeoms);
      return true;
    } else {
      spdlog::error(
          "First message must have header \'{:s}\', got \'{:s}\'. Retry.",
          CMD_send_models, msg_header);
      continue;
    }
  }
  return false;
}

void pull_socket_router(Visualizer &viz, std::span<zmq::message_t, 2> msgs,
                        zmq::socket_ref sync_sock) {
  auto header = msgs[0].to_string_view();
  if (header == CMD_send_cam_pose) {
    auto oh = get_handle_from_zmq_msg(std::move(msgs[1]));
    msgpack::object obj = oh.get();
    ArrayMessage M_msg = obj.as<ArrayMessage>();
    auto M = get_eigen_view_from_spec<RowMat4d>(M_msg);
    viz.setCameraPose(M);
    sync_sock.send(zmq::str_buffer("ok"));
  } else if (header == CMD_reset_camera) {
    viz.resetCamera();
    sync_sock.send(zmq::str_buffer("ok"));
  } else if (header == CMD_clean) {
    viz.clean();
    sync_sock.send(zmq::str_buffer("ok"));
  } else if (header == CMD_toggle_gui) {
    viz.toggleGui();
    sync_sock.send(zmq::str_buffer("ok"));
  } else if (header == CMD_send_models) {
    sync_sock.send(
        zmq::str_buffer("error: visualizer already has models open."));
  } else if (header == CMD_start_recording) {
    auto filename = msgs[1].to_string_view();
    try {
      viz.startRecording(filename);
      sync_sock.send(zmq::str_buffer("ok"));
    } catch (const std::runtime_error &err) {
      std::string err_msg{err.what()};
      sync_sock.send(zmq::message_t(err_msg));
    }
  } else if (header == CMD_stop_recording) {
    char data{viz.stopRecording()};
    sync_sock.send(zmq::buffer(&data, 1));
  }
}

void run_main_loop(Visualizer &viz, ApplicationContext &app_ctx) {

  while (!viz.shouldExit()) {
    std::array<zmq::message_t, 2> msgs;

    // route subscriber socket
    auto rec = zmq::recv_multipart_n(app_ctx.state_sock, msgs.begin(), 2,
                                     zmq::recv_flags::dontwait);

    if (rec) {
      // route through message headers
      auto header = msgs[0].to_string_view();
      if (header == CMD_state_update) {
        auto oh = get_handle_from_zmq_msg(std::move(msgs[1]));
        msgpack::object obj = oh.get();
        std::tuple<ArrayMessage, std::optional<ArrayMessage>> arrays;
        obj.convert(arrays);
        auto [q_msg, v_msg] = arrays;
        auto q_view = get_eigen_view_from_spec<Eigen::VectorXd>(q_msg);
        if (!v_msg) {
          pin::forwardKinematics(viz.model(), viz.data(), q_view);
        } else {
          auto v_view = get_eigen_view_from_spec<Eigen::VectorXd>(*v_msg);
          pin::forwardKinematics(viz.model(), viz.data(), q_view, v_view);
        }
      }
    }

    // route synchronous socket
    // reuse our buffer for messages.
    rec = zmq::recv_multipart_n(app_ctx.sync_sock, msgs.begin(), 2,
                                zmq::recv_flags::dontwait);
    if (rec) {
      pull_socket_router(viz, msgs, app_ctx.sync_sock);
    }

    viz.display();
  }
}

int main(int argc, char *argv[]) {
  spdlog::cfg::load_env_levels();
  spdlog::set_pattern(">>> [%T] [%^%l%$] %v");
  CLI::App app{"Candlewick visualizer runtime"};
  argv = app.ensure_utf8(argv);

  std::array<Uint32, 2> window_dims{1920u, 1080u};
  app.add_option("--dims", window_dims, "Window dimensions.")
      ->capture_default_str();

  std::string hostname = "127.0.0.1";
  app.add_option("--host", hostname, "Host name.")->capture_default_str();
  Uint16 port = 12000;
  app.add_option("-p,--port", port, "Base port")->capture_default_str();

  CLI11_PARSE(app, argc, argv);

  // ===== Runtime application context =====
  ApplicationContext app_ctx;
  zmq::socket_t &sync_sock = app_ctx.sync_sock;
  zmq::socket_t &state_sock = app_ctx.state_sock;
  sync_sock.bind(std::format("tcp://{:s}:{:d}", hostname, port));
  state_sock.bind(std::format("tcp://{:s}:{:d}", hostname, port + 2));
  state_sock.set(zmq::sockopt::subscribe, CMD_state_update);

  std::string endpoint;
  endpoint = sync_sock.get(zmq::sockopt::last_endpoint);
  spdlog::info("ZMQ endpoint (setup): {:s}", endpoint);
  endpoint = state_sock.get(zmq::sockopt::last_endpoint);
  spdlog::info("ZMQ endpoint (state): {:s}", endpoint);

  // Handle first message
  bool loaded_models = handle_first_message(app_ctx);
  if (!loaded_models)
    return 1;

  Visualizer::Config config;
  config.width = window_dims[0];
  config.height = window_dims[1];
  Visualizer viz{config, app_ctx.model, app_ctx.visual_model};

  run_main_loop(viz, app_ctx);

  return 0;
}
