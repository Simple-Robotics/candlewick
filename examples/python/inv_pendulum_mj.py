import pinocchio as pin

from candlewick import Visualizer, VisualizerConfig


filename = "inv_pendulum.xml"
rets = pin.buildModelsFromMJCF(filename)
if len(rets) == 3:
    model, _, visual_model = rets
else:
    model, _, visual_model, _ = rets


config = VisualizerConfig()
config.width = 1280
config.height = 720
viz = Visualizer(config, model, visual_model)

q = pin.neutral(model)

while not viz.shouldExit:
    viz.display(q)
