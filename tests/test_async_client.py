import pinocchio as pin
import example_robot_data as erd
import zmq
import numpy as np
import msgspec

from typing import Any, Type


NUMPY_TYPE_CODE = 1
PORT = 12000


class NumpySpec(msgspec.Struct, gc=False, array_like=True):
    dtype: str
    shape: tuple[int, ...]
    data: bytes

    def to_numpy(self):
        return np.frombuffer(self.data, dtype=self.dtype).reshape(self.shape)

    @classmethod
    def from_numpy(cls, arr: np.ndarray) -> "NumpySpec":
        return cls(str(arr.dtype), arr.shape, arr.data)


def numpy_enc_hook(obj: Any):
    if isinstance(obj, np.ndarray):
        return NumpySpec.from_numpy(obj)
    return obj


def numpy_dec_hook(type: Type, obj: Any):
    print("dec_hook")
    print(type, obj)
    if type is NumpySpec:
        return obj.to_numpy()
    else:
        raise NotImplementedError("Other objects supported")


_encoder = msgspec.msgpack.Encoder(enc_hook=numpy_enc_hook)
_decoder = msgspec.msgpack.Decoder(dec_hook=numpy_dec_hook)


def send_models(sock: zmq.Socket, model_str, geom_str):
    """Send Pinocchio model and geometry model as strings."""
    sock.send_multipart([b"send_models", _encoder.encode([model_str, geom_str])])


def send_state(sock: zmq.Socket, q: np.ndarray, v: np.ndarray | None = None):
    payload = _encoder.encode((q, v))
    sock.send_multipart([b"state_update", payload])


def send_cam_pose(sock: zmq.Socket, M: np.ndarray):
    assert sock.socket_type == zmq.REQ
    sock.send_multipart([b"send_cam_pose", _encoder.encode(M)])
    sock.recv()


def cmd_clean(sock: zmq.Socket):
    """Clean the robot from the renderer. Equivalent to `viz.clean()` on the synchronous `Visualizer` class."""
    assert sock.socket_type == zmq.REQ
    sock.send_multipart([b"cmd_clean", b""])
    sock.recv()


if __name__ == "__main__":
    import time

    ctx = zmq.Context.instance()
    sock1: zmq.Socket = ctx.socket(zmq.SocketType.REQ)
    url1 = f"tcp://127.0.0.1:{PORT}"
    url2 = f"tcp://127.0.0.1:{PORT + 2}"
    sock1.connect(url1)
    sock2: zmq.Socket = ctx.socket(zmq.SocketType.PUB)
    sock2.connect(url2)

    robot: pin.RobotWrapper = erd.load("ur3")
    model: pin.Model = robot.model
    geom_model: pin.GeometryModel = robot.visual_model

    model_str = model.saveToString()
    geom_str = geom_model.saveToString()
    send_models(sock1, model_str, geom_str)
    response = sock1.recv().decode()
    print(response)

    # q0 = pin.neutral(model)
    q0 = pin.randomConfiguration(model)
    send_state(sock2, q0)
    q1 = pin.randomConfiguration(model)
    _dt = 0.02
    _v = pin.difference(model, q0, q1) / _dt

    def f(i, vel=False):
        time.sleep(_dt)
        t_ = _dt * i
        freq = 1e-2
        print("Time:", t_)
        ph = np.sin(_dt * i * freq)
        q = pin.interpolate(model, q0, q1, ph)
        if vel:
            send_state(sock2, q, _v)
        else:
            send_state(sock2, q)

    for i in range(400):
        f(i)

    # test setting camera
    input("[enter] to set camera pose")
    Mref = np.eye(4)
    Mref[:3, 3] = (0.05, 0, 2.0)
    send_cam_pose(sock1, Mref)
