from contextlib import contextmanager
from . import Visualizer, hasFfmpegSupport
from typing import Union, TYPE_CHECKING
import warnings

if TYPE_CHECKING:
    from .async_visualizer import AsyncVisualizer

_DEFAULT_VIDEO_SETTINGS = {"fps": 30, "bitRate": 3_000_000}

__all__ = ["create_recorder_context"]


@contextmanager
def create_recorder_context(
    viz: Union[Visualizer, "AsyncVisualizer"],
    filename: str,
    /,
    fps: int = _DEFAULT_VIDEO_SETTINGS["fps"],
    bitRate: int = _DEFAULT_VIDEO_SETTINGS["bitRate"],
):
    filename = str(filename)
    if not hasFfmpegSupport():
        warnings.warn(
            "This context will do nothing, as Candlewick was built without video recording support."
        )
    else:
        if isinstance(viz, Visualizer):
            viz.videoSettings().fps = fps
            viz.videoSettings().bitRate = bitRate
        viz.startRecording(filename)
    try:
        yield
    finally:
        viz.stopRecording()
