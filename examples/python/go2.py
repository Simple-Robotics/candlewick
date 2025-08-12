import pinocchio as pin
import candlewick as cdw
import tqdm

from candlewick import Visualizer, VisualizerConfig
from utils import add_floor_geom

try:
    from go2_description.loader import loadGo2
except ImportError as import_error:
    raise ImportError(
        "This example requires the go2_description package, which was not found. "
        "Download it at: https://github.com/inria-paris-robotics-lab/go2_description"
    ) from import_error


print(f"Current shader directory: {cdw.currentShaderDirectory()}")

robot = loadGo2()

rmodel: pin.Model = robot.model
rdata: pin.Data = robot.data
visual_model: pin.GeometryModel = robot.visual_model
add_floor_geom(visual_model)
visual_data = visual_model.createData()

q0 = rmodel.referenceConfigurations["standing"]
dt = 0.01

config = VisualizerConfig()
config.width = 2400
config.height = 900
viz = Visualizer(config, rmodel, visual_model, data=rdata, visual_data=visual_data)
assert viz.hasExternalData()

print(
    "Visualizer has renderer:",
    viz.renderer,
    "driver name:",
    viz.renderer.device.driverName(),
)
print("Scene bounds:", viz.worldSceneBounds)

with cdw.video_context.create_recorder_context(viz, "go2_record.mp4"):
    for i in tqdm.tqdm(range(1000)):
        if viz.shouldExit:
            break
        viz.display(q0)
        # time.sleep(dt)

print("Goodbye...")
