import pinocchio as pin
import time
import numpy as np
from utils import add_floor_geom
from candlewick import Visualizer, VisualizerConfig

try:
    import example_robot_data as erd
except ImportError as import_error:
    raise ImportError(
        "example-robot-data package not found. Please install "
        "it (from e.g. pip or conda)."
    ) from import_error

robot = erd.load("ur10")
model: pin.Model = robot.model
data: pin.Data = robot.data
visual_model: pin.GeometryModel = robot.visual_model
add_floor_geom(visual_model)
visual_data = visual_model.createData()

config = VisualizerConfig()
config.width = 1920
config.height = 1080
viz = Visualizer(config, model, visual_model, data=data, visual_data=visual_data)

q0 = pin.neutral(model)
v0 = np.zeros(model.nv)

t = 0.0
dt = 0.02
frame_id = model.getFrameId("elbow_joint")
joint_id = model.getJointId("elbow_joint")
viz.addFrameViz(model.getFrameId("ee_link"), show_velocity=True)
q = q0.copy()
v = v0.copy()

i = 0


def get_force(t: float):
    ef = np.zeros(3)
    ef[1] = 1.2 * np.cos(2 * t)
    return ef


while not viz.shouldExit and i < 1000:
    alpha = np.sin(t)
    ext_forces = [pin.Force.Zero() for _ in range(model.njoints)]
    dw = np.random.randn(3)
    f = ext_forces[joint_id]
    f.linear[:] = get_force(t)
    if i == 0:
        viz.setFrameExternalForce(frame_id, force=f, lifetime=100)
    elif i > 100:
        viz.setFrameExternalForce(frame_id, f)

    a_des = np.zeros(model.nv)
    u0 = pin.rnea(model, data, q, v, a_des)
    a = pin.aba(model, data, q, v, u0, ext_forces)

    pin.forwardKinematics(model, data, q, v)
    viz.display()

    time.sleep(dt)
    t += dt
    v += dt * a
    q = pin.integrate(model, q, v * dt)
    i += 1
