"""
Demo for the AsyncVisualizer class, e.g. a client which connects
to the candlewick visualizer runtime.
"""

import pinocchio as pin
import numpy as np
import warnings

from candlewick import create_recorder_context
from candlewick.async_visualizer import AsyncVisualizer

try:
    import example_robot_data as erd
except ImportError as import_error:
    raise ImportError(
        "example-robot-data package not found. Please install "
        "it (from e.g. pip or conda)."
    ) from import_error


if __name__ == "__main__":
    import time

    robot: pin.RobotWrapper = erd.load("ur3")
    model: pin.Model = robot.model
    geom_model: pin.GeometryModel = robot.visual_model

    client = AsyncVisualizer(model, geom_model)
    warnings.warn(
        "You will need to launch the candlewick runtime on the appropriate host and port."
    )
    client.initViewer()
    client.loadViewerModel()

    q0 = pin.randomConfiguration(model)
    q1 = pin.randomConfiguration(model)
    _dt = 0.02
    _v = pin.difference(model, q0, q1) / _dt
    client.display(q0)

    def f(i, vel=False):
        time.sleep(_dt)
        t_ = _dt * i
        freq = 1e-2
        print("Time:", t_)
        ph = np.sin(_dt * i * freq)
        q = pin.interpolate(model, q0, q1, ph)
        client.display(q, _v if vel else None)

    with create_recorder_context(client, "async_record.mp4"):
        for i in range(400):
            f(i)

    # test setting camera
    input("[enter] to set camera pose")
    Mref = np.eye(4)
    Mref[:3, 3] = (0.05, 0, 2.0)
    client.setCameraPose(pose=Mref)

    input("[enter] to reset camera")
    client.resetCamera()
