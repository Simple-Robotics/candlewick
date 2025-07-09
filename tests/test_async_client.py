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
    payload = _encoder.encode(q)
    sock.send_multipart([b"send_state", payload])


if __name__ == "__main__":
    ctx = zmq.Context.instance()
    sock: zmq.Socket = ctx.socket(zmq.SocketType.REQ)
    url = f"tcp://127.0.0.1:{PORT}"
    sock.connect(addr=url)

    robot: pin.RobotWrapper = erd.load("ur3")
    model: pin.Model = robot.model
    geom_model: pin.GeometryModel = robot.visual_model

    model_str = model.saveToString()
    geom_str = geom_model.saveToString()
    send_models(sock, model_str, geom_str)
    response = sock.recv().decode()
    print(response)

    # q0 = pin.neutral(model)
    q0 = pin.randomConfiguration(model)
    send_state(sock, q0)
    response = sock.recv().decode()
    print(response)
