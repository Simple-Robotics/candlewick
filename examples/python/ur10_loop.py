import example_robot_data as erd
import pinocchio as pin
import time
import numpy as np
from candlewick import Visualizer, VisualizerConfig

robot = erd.load("ur10")
model: pin.Model = robot.model
data: pin.Data = robot.data
visual_model = robot.visual_model

config = VisualizerConfig()
config.width = 1920
config.height = 1080
viz = Visualizer(config, model, visual_model)

q0 = pin.neutral(model)
q1 = pin.randomConfiguration(model)

t = 0.0
dt = 0.02
viz.addFrameViz(frame_id=model.getFrameId("ee_link"))

i = 0
while not viz.shouldExit and i < 1000:
    alpha = np.sin(t)
    q = pin.interpolate(model, q0, q1, alpha)
    pin.framesForwardKinematics(model, data, q)
    viz.display(q)
    time.sleep(dt)
    t += dt
    i += 1
