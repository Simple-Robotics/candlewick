import example_robot_data as erd
import pinocchio as pin
import time
import numpy as np
from candlewick.multibody import Visualizer, VisualizerConfig

robot = erd.load("ur10")
model: pin.Model = robot.model
data: pin.Data = robot.data
visual_model = robot.visual_model

config = VisualizerConfig()
config.width = 1280
config.height = 720
viz = Visualizer(config, model, visual_model)

print(
    "Visualizer has renderer:",
    viz.renderer,
    "driver name:",
    viz.renderer.device.driverName(),
)

q0 = pin.neutral(model)
q1 = pin.randomConfiguration(model)

t = 0.0
dt = 0.02
M = pin.SE3.Identity()
ee_name = "ee_link"
ee_id = model.getFrameId(ee_name)

for i in range(1000):
    alpha = np.sin(t)
    q = pin.interpolate(model, q0, q1, alpha)
    pin.framesForwardKinematics(model, data, q)
    M = data.oMf[ee_id]
    if viz.shouldExit:
        break
    viz.display(q)
    time.sleep(dt)
    t += dt
