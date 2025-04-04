#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# SPDX-License-Identifier: Apache-2.0
# Copyright 2025 Inria

"""
Load a robot description selected from the command line in Candlewick.

This example requires Pinocchio, installed by e.g. `conda install pinocchio`,
and `robot_descriptions`, installed by e.g. `conda install robot_descriptions`.
"""

import argparse

import pinocchio as pin

from candlewick.multibody import Visualizer, VisualizerConfig

try:
    from robot_descriptions.loaders.pinocchio import load_robot_description
except ImportError as import_error:
    raise ImportError(
        "Robot descriptions package not found, "
        "install it by e.g. `conda install robot_descriptions`."
    ) from import_error

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name", help="name of the robot description")
    parser.add_argument(
        "--width", help="window width in px", type=int, default=1600,
    )
    parser.add_argument(
        "--height", help="window height in px", type=int, default=900,
    )
    parser.add_argument(
        "--altitude",
        help="robot altitude offset in [m]",
        type=float,
        default=0.0,
    )
    args = parser.parse_args()

    try:
        robot = load_robot_description(
            args.name, root_joint=pin.JointModelFreeFlyer()
        )
    except ModuleNotFoundError:
        robot = load_robot_description(
            f"{args.name}_description", root_joint=pin.JointModelFreeFlyer()
        )

    config = VisualizerConfig()
    config.width = args.width
    config.height = args.height
    visualizer = Visualizer(config, robot.model, robot.visual_model)
    q = robot.q0.copy()
    q[2] += float(args.altitude)
    while not visualizer.shouldExit:
        visualizer.display(q)
