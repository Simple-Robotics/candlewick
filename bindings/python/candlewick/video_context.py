from .pycandlewick.multibody import Visualizer


class visualizer_video_context_wrapper:
    def __init__(self, viz: Visualizer, filename: str, fps: int):
        self.visualizer = viz
        self.filename = filename
        self.fps = fps

    def __enter__(self):
        self.visualizer.startRecording(filename=self.filename, fps=self.fps)

    def __exit__(self):
        self.visualizer.stopRecording()


def create_recorder_context(viz: Visualizer, filename: str, /, fps: int = 30):
    return visualizer_video_context_wrapper(viz, filename, fps)


__all__ = ["create_recorder_context"]
