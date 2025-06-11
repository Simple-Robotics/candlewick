from . import Visualizer
from . import VideoRecorderSettings
from contextlib import contextmanager


_DEFAULT_VIDEO_SETTINGS = VideoRecorderSettings()


@contextmanager
def create_recorder_context(
    viz: Visualizer,
    filename: str,
    /,
    fps: int = _DEFAULT_VIDEO_SETTINGS.fps,
    bitRate: int = _DEFAULT_VIDEO_SETTINGS.bitRate,
):
    viz.videoSettings().fps = fps
    viz.videoSettings().bitRate = bitRate
    viz.startRecording(filename)
    try:
        yield
    finally:
        viz.stopRecording()


__all__ = ["create_recorder_context"]
