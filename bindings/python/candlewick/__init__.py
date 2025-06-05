# ruff: noqa: F401, F403, F405
from .pycandlewick import *  # noqa
from .pycandlewick import __version__


def _process():
    import sys
    import inspect
    from pathlib import Path
    from . import pycandlewick

    lib_name = "candlewick"
    submodules = inspect.getmembers(pycandlewick, inspect.ismodule)
    for mod_info in submodules:
        mod_name = "{}.{}".format(lib_name, mod_info[0])
        sys.modules[mod_name] = mod_info[1]
        mod_info[1].__file__ = pycandlewick.__file__
        mod_info[1].__name__ = mod_name

    share_dir = Path(__file__).parent / "../../../../share"
    shaders_dir = share_dir.resolve() / "candlewick" / "shaders"
    compiled_shaders_dir = shaders_dir / "compiled"
    pycandlewick.setShadersDirectory(str(compiled_shaders_dir))


_process()
