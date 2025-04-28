# ruff: noqa: F401, F403, F405
from .pycandlewick import *  # noqa
from .pycandlewick import __version__


def _process():
    import sys
    import inspect
    import os.path
    from . import pycandlewick

    lib_name = "candlewick"
    submodules = inspect.getmembers(pycandlewick, inspect.ismodule)
    for mod_info in submodules:
        mod_name = "{}.{}".format(lib_name, mod_info[0])
        sys.modules[mod_name] = mod_info[1]
        mod_info[1].__file__ = pycandlewick.__file__
        mod_info[1].__name__ = mod_name

    share_dir = os.path.join(os.path.dirname(__file__), "../../../../share")
    shaders_dir = os.path.join(share_dir, "candlewick", "shaders")
    shaders_dir = os.path.abspath(shaders_dir)
    compiled_shaders_dir = os.path.join(shaders_dir, "compiled")
    pycandlewick.setShadersDirectory(compiled_shaders_dir)


_process()
