import candlewick as cdw
from candlewick.multibody import Visualizer, VisualizerConfig

try:
    import go2_description as go2d
except ImportError:
    import warnings

    warnings.warn(
        "This example requires the go2_description package, which was not found. Download it at: https://github.com/inria-paris-robotics-lab/go2_description"
    )
    raise
import time
import tqdm


print(f"Current shader directory: {cdw.currentShaderDirectory()}")

robot = go2d.loadGo2()

rmodel = robot.model
rdata = robot.data
visual_model = robot.visual_model
visual_data = robot.visual_data

q0 = rmodel.referenceConfigurations["standing"]
dt = 0.01

config = VisualizerConfig()
config.width = 1600
config.height = 900
viz = Visualizer(config, rmodel, visual_model, data=rdata, visual_data=visual_data)
assert viz.hasExternalData

print(
    "Visualizer has renderer:",
    viz.renderer,
    "driver name:",
    viz.renderer.device.driverName(),
)
print("Scene bounds:", viz.worldSceneBounds)

viz.startRecording("go2_record.mp4")

for i in tqdm.tqdm(range(1000)):
    if viz.shouldExit:
        break
    viz.display(q0)
    time.sleep(dt)

viz.stopRecording()
print("Goodbye...")
