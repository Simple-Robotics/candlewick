#include "candlewick/multibody/Multibody.h"
#include "candlewick/multibody/Visualizer.h"

#include <pinocchio/serialization/model.hpp>
#include <pinocchio/serialization/geometry.hpp>
#include <pinocchio/algorithm/kinematics.hpp>

#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <msgpack.hpp>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

namespace cdw = candlewick;
namespace pin = pinocchio;
using cdw::multibody::Visualizer;

constexpr std::string_view CMD_SEND_MODELS = "send_models";
constexpr std::string_view CMD_SEND_STATE = "state_update";
constexpr std::string_view CMD_SEND_CAM_POSE = "send_cam_pose";
constexpr std::string_view CMD_CLEAN = "cmd_clean";
using RowMat4d = Eigen::Matrix<pin::context::Scalar, 4, 4, Eigen::RowMajor>;

msgpack::object_handle get_handle_from_zmq_msg(zmq::message_t &&msg) {
  if (!msg.empty())
    return msgpack::unpack(static_cast<const char *>(msg.data()), msg.size());
  else
    return msgpack::object_handle();
}

struct ArrayMessage {
  std::string dtype;
  std::vector<long> dims;
  std::vector<uint8_t> data;

  size_t ndim() const noexcept { return dims.size(); }

  MSGPACK_DEFINE(dtype, dims, data);
};

namespace detail {
template <typename MapType, typename P>
auto get_eigen_view_msg_impl(const ArrayMessage &spec, P *data) {
  using PlainObject = typename MapType::PlainObject;
  const Eigen::Index rows = spec.dims[0];
  if constexpr (PlainObject::IsVectorAtCompileTime) {
    return MapType{data, rows};
  } else {
    const Eigen::Index cols = spec.ndim() == 2
                                  ? spec.dims[1]
                                  : 1; // assume runtime vector if ndim wrong
    return MapType{data, rows, cols};
  }
}
} // namespace detail

/// \brief Convert ArrayMessage to a mutable Eigen::Matrix view.
template <typename MatrixType>
auto get_eigen_view_from_spec(ArrayMessage &spec) {
  using Scalar = typename MatrixType::Scalar;
  using MapType = Eigen::Map<MatrixType>;
  Scalar *data = reinterpret_cast<Scalar *>(spec.data.data());
  return detail::get_eigen_view_msg_impl<MapType>(spec, data);
}

/// \copydoc get_eigen_view_from_spec()
template <typename MatrixType>
auto get_eigen_view_from_spec(const ArrayMessage &spec) {
  using Scalar = typename MatrixType::Scalar;
  using MapType = Eigen::Map<const MatrixType>;
  const Scalar *data = reinterpret_cast<const Scalar *>(spec.data.data());
  return detail::get_eigen_view_msg_impl<MapType>(spec, data);
}

struct ApplicationContext {
  pin::Model model;
  pin::GeometryModel geom_model;
  zmq::context_t ctx{};
  zmq::socket_t sync_sock{ctx, zmq::socket_type::rep};
  zmq::socket_t state_sock{ctx, zmq::socket_type::sub};
};

/// Handle first incoming message.
bool handle_first_message(zmq::socket_ref sock, ApplicationContext &app_ctx) {
  while (true) {
    std::array<zmq::message_t, 2> msgs;

    auto ret = zmq::recv_multipart_n(sock, msgs.begin(), 2);
    if (!ret)
      return false;

    auto msg_header = msgs[0].to_string_view();
    if (msg_header == CMD_SEND_MODELS) {
      msgpack::object_handle oh = get_handle_from_zmq_msg(std::move(msgs[1]));
      auto obj = oh.get();
      std::array<std::string, 2> model_strings;
      obj.convert(model_strings);
      app_ctx.model.loadFromString(model_strings[0]);
      app_ctx.geom_model.loadFromString(model_strings[1]);
      // send response only when models loaded.
      sock.send(zmq::str_buffer("ok"));

      SDL_Log("Loaded model with %d joints", app_ctx.model.njoints);
      SDL_Log("Loaded geometry model with %zd gobjs",
              app_ctx.geom_model.ngeoms);
      return true;
    } else {
      SDL_Log("First message must have header \'%s\', got \'%s\'. Retry.",
              CMD_SEND_MODELS.data(), msg_header.data());
      continue;
    }
  }
  return false;
}

void run_main_loop(Visualizer &viz, ApplicationContext &app_ctx) {

  while (!viz.shouldExit()) {
    std::array<zmq::message_t, 2> msgs;
    auto rec = zmq::recv_multipart_n(app_ctx.state_sock, msgs.begin(), 2,
                                     zmq::recv_flags::dontwait);

    if (rec) {
      // route through message headers
      auto header = msgs[0].to_string_view();
      if (header == CMD_SEND_STATE) {
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

    rec = zmq::recv_multipart_n(app_ctx.sync_sock, msgs.begin(), 2,
                                zmq::recv_flags::dontwait);
    if (rec) {
      auto header = msgs[0].to_string_view();
      if (header == CMD_SEND_CAM_POSE) {
        auto oh = get_handle_from_zmq_msg(std::move(msgs[1]));
        msgpack::object obj = oh.get();
        ArrayMessage M_msg = obj.as<ArrayMessage>();
        auto M = get_eigen_view_from_spec<RowMat4d>(M_msg);
        viz.setCameraPose(M);
        app_ctx.sync_sock.send(zmq::str_buffer("ok"));
      } else if (header == CMD_CLEAN) {
        viz.clean();
        app_ctx.sync_sock.send(zmq::str_buffer("ok"));
      }
    }

    viz.display();
  }
}

int main(int argc, char **argv) {
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
  state_sock.set(zmq::sockopt::subscribe, CMD_SEND_STATE);

  std::string endpoint;
  endpoint = sync_sock.get(zmq::sockopt::last_endpoint);
  SDL_Log("ZMQ endpoint (setup): %s", endpoint.c_str());
  endpoint = state_sock.get(zmq::sockopt::last_endpoint);
  SDL_Log("ZMQ endpoint (state): %s", endpoint.c_str());

  // Handle first message
  bool loaded_models = handle_first_message(sync_sock, app_ctx);
  if (!loaded_models)
    return 1;

  Visualizer::Config config;
  config.width = window_dims[0];
  config.height = window_dims[1];
  Visualizer viz{config, app_ctx.model, app_ctx.geom_model};

  run_main_loop(viz, app_ctx);

  return 0;
}
