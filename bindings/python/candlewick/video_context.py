from . import Visualizer
from contextlib import contextmanager


@contextmanager
def create_recorder_context(viz: Visualizer, filename: str, /, fps: int = 30):
    viz.startRecording(filename, fps)
    try:
        yield
    finally:
        viz.stopRecording()


__all__ = ["create_recorder_context"]
