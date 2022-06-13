# @file
# Setup script to freeze ConfigEditor.
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import os
from cx_Freeze import setup, Executable

# Dependencies are automatically detected, but it might need fine tuning.
build_exe_options = {"packages": ["os"]}

setup(
    name="MuConfigEditor",
    version="1.0",
    description="",
    options={"build_exe": build_exe_options},
    executables=[
        Executable(
            os.path.join(os.path.dirname(os.path.realpath(__file__)), "ConfigEditor.py")
        )
    ],
)
