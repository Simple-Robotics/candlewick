import pinocchio as pin

from candlewick import Visualizer, VisualizerConfig


filename = "inv_pendulum.xml"
model, geom_model, visual_model = pin.buildModelsFromMJCF(filename)

config = VisualizerConfig()
config.width = 1280
config.height = 720
viz = Visualizer(config, model, visual_model)

q = pin.neutral(model)

while not viz.shouldExit:
    viz.display(q)
