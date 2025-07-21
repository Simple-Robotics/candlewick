import pinocchio as pin
import zmq
import numpy as np
import msgspec

from pinocchio.visualize import BaseVisualizer
from typing import Any, Type, Optional


NUMPY_TYPE_CODE = 1
DEFAULT_PORT = 12000


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
    return sock.recv()


def send_state(sock: zmq.Socket, q: np.ndarray, v: np.ndarray | None = None):
    payload = _encoder.encode((q, v))
    sock.send_multipart([b"state_update", payload])


def send_cam_pose(sock: zmq.Socket, M: np.ndarray):
    assert sock.socket_type == zmq.REQ
    assert M.shape == (4, 4)
    sock.send_multipart([b"send_cam_pose", _encoder.encode(M)])
    return sock.recv()


class AsyncVisualizer(BaseVisualizer):
    """A visualizer-like client for the candlewick runtime."""

    zmq_ctx: zmq.Context
    sync_sock: zmq.Socket
    publisher: zmq.Socket

    def __init__(self, model: pin.Model, visual_model: pin.GeometryModel):
        super().__init__(model=model, visual_model=visual_model)

        self.zmq_ctx = zmq.Context.instance()
        self.sync_sock = self.zmq_ctx.socket(zmq.REQ)
        self.publisher = self.zmq_ctx.socket(zmq.PUB)
        self.model: pin.Model
        self.visual_model: pin.GeometryModel

    def initViewer(self, loadModel=False, /, hostname="127.0.0.1", port=DEFAULT_PORT):
        url1 = f"tcp://{hostname}:{port}"
        url2 = f"tcp://{hostname}:{port + 2}"
        self.sync_sock.connect(addr=url1)
        self.publisher.connect(addr=url2)
        print(f"Connected sync socket {self.sync_sock.socket_type} to {url1}")
        print(f"Connected state publisher {self.publisher.socket_type} to {url2}")

        if loadModel:
            self.loadViewerModel()

    def loadViewerModel(self):
        """This function will serialize"""
        model_str = self.model.saveToString()
        geom_str = self.visual_model.saveToString()
        response = send_models(self.sync_sock, model_str, geom_str).decode()
        assert response == "ok"

    def rebuildData(self):
        raise NotImplementedError(
            f"""This method is not implemented. {type(self).__name__} does not "
            "directly interact with Pinocchio Data or GeometryData objects."""
        )

    def setCameraPose(self, pose: np.ndarray):
        send_cam_pose(self.sync_sock, pose)

    def resetCamera(self):
        self.sync_sock.send_multipart([b"reset_camera", b""])
        response = self.sync_sock.recv().decode()
        assert response == "ok"

    def display(self, q: np.ndarray, v: Optional[np.ndarray] = None):
        """Publish the robot state."""
        assert q.size == self.model.nq
        if v is not None:
            assert v.size == self.model.nv
        send_state(self.publisher, q, v)

    def clean(self):
        """Clean the robot from the renderer. Equivalent to `viz.clean()` on the synchronous `Visualizer` class."""
        self.sync_sock.send_multipart([b"cmd_clean", b""])
        response = self.sync_sock.recv().decode()
        assert response == "ok"

    def close(self):
        self.clean()
        self.publisher.close()
        self.sync_sock.close()

    def displayCollisions(self, visibility):
        raise NotImplementedError()

    def displayVisuals(self, visibility):
        raise NotImplementedError()

    def setBackgroundColor(self):
        raise NotImplementedError()

    def setCameraTarget(self, target):
        raise NotImplementedError()

    def setCameraPosition(self, position: np.ndarray):
        raise NotImplementedError()

    def setCameraZoom(self, zoom: float):
        raise NotImplementedError()

    def captureImage(self, w=None, h=None):
        raise NotImplementedError()

    def disableCameraControl(self):
        raise NotImplementedError()

    def enableCameraControl(self):
        raise NotImplementedError()

    def drawFrameVelocities(self, *args, **kwargs):
        raise NotImplementedError()

    def startRecording(self, filename: str):
        """Open a recording to a given file."""
        self.sync_sock.send_multipart([b"start_recording", filename.encode("utf-8")])
        response = self.sync_sock.recv().decode("utf-8")
        if response != "ok":
            print(f"Visualizer runtime error: {response}")

    def stopRecording(self):
        """
        Stop the recording if it's running.

        :returns: Whether a recording was actually stopped.
        """
        self.sync_sock.send_multipart([b"stop_recording", b""])
        return bool(self.sync_sock.recv())
