# Script to compile Slang shaders using slangc.
import subprocess
import argparse
import pathlib as pt
import sys

# Maps file extension abbreviation -> slangc -stage argument
STAGE_MAP = {
    "vert": "vertex",
    "frag": "fragment",
    "comp": "compute",
    "geom": "geometry",
    "tesc": "hull",
    "tese": "domain",
}

parser = argparse.ArgumentParser(description="Compile Slang shaders using slangc.")
parser.add_argument("shader_name", help="Base shader name, e.g. 'PbrBasic'.")
group = parser.add_mutually_exclusive_group(required=True)
group.add_argument(
    "--stages",
    nargs="+",
    metavar="STAGE",
    help="Stage abbreviations to compile (vert, frag, comp, ...).",
)
group.add_argument(
    "--all-stages",
    "-a",
    action="store_true",
    help="Process all .slang stage files found for the shader.",
)
parser.add_argument(
    "--spv-only",
    action="store_true",
    help="Only emit SPIR-V and reflection JSON; skip MSL output.",
)
args = parser.parse_args()

SHADER_SRC_DIR = pt.Path("shaders/src")
SHADER_OUT_DIR = pt.Path("shaders/compiled")

print("Shader src dir:", SHADER_SRC_DIR.absolute())

if args.all_stages:
    # Glob for PbrBasic.vert.slang, PbrBasic.frag.slang, etc.
    stage_files = sorted(SHADER_SRC_DIR.glob(f"{args.shader_name}.[a-z]*.slang"))
else:
    stage_files = [
        SHADER_SRC_DIR / f"{args.shader_name}.{s}.slang" for s in args.stages
    ]

assert len(stage_files) > 0, f"No stage files found for '{args.shader_name}'!"
print(f"Processing: {[f.name for f in stage_files]}")


def stage_from_path(path: pt.Path) -> str:
    """PbrBasic.vert.slang -> 'vertex'"""
    abbrev = path.suffixes[-2].lstrip(".")  # second-to-last suffix
    stage = STAGE_MAP.get(abbrev)
    if stage is None:
        print(f"WARNING: unknown stage abbreviation '{abbrev}', passing as-is.")
        stage = abbrev
    return stage


def run(cmd: list) -> None:
    print("+", " ".join(str(x) for x in cmd))
    result = subprocess.run(cmd, shell=False)
    if result.returncode != 0:
        print(f"ERROR: slangc failed (exit {result.returncode})", file=sys.stderr)
        sys.exit(result.returncode)


for stage_file in stage_files:
    if not stage_file.exists():
        print(f"ERROR: file not found: {stage_file}", file=sys.stderr)
        sys.exit(1)

    stage = stage_from_path(stage_file)

    # Strip trailing .slang: "PbrBasic.vert.slang" -> "PbrBasic.vert"
    base_name = stage_file.name.removesuffix(".slang")

    spv_file = SHADER_OUT_DIR / (base_name + ".spv")
    msl_file = SHADER_OUT_DIR / (base_name + ".msl")
    json_file = SHADER_OUT_DIR / (base_name + ".json")

    base_cmd = [
        "slangc",
        f"-I{SHADER_SRC_DIR}",
        stage_file,
        "-stage",
        stage,
        "-entry",
        "main",
    ]

    print(f"\n--- {stage_file.name} ---")

    run(base_cmd + ["-target", "spirv", "-o", spv_file, "-reflection-json", json_file])
    print(f"  => {spv_file}")
    print(f"  => {json_file}")

    if not args.spv_only:
        run(base_cmd + ["-target", "metal", "-o", msl_file])
        print(f"  => {msl_file}")
