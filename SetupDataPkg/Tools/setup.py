# @file
# Setup script to publish binaries for ConfigEditor, ReadUefiVarsToConfVarList, and WriteConfVarListToUefiVars.
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import os
import sys
import argparse
import logging
import PyInstaller.__main__


def build_executable(script_name, script_path, conf_folder_path, output_file_path):
    print(
        f"Building executable for script: {script_name} at path: {script_path}"
    )  # Debugging line
    args = [script_path, "--clean", "--onefile"]

    if conf_folder_path is not None:
        for subdir, _, files in os.walk(conf_folder_path):
            for file in files:
                sub_path = os.path.join(subdir, file)
                rel_path = os.path.relpath(subdir, conf_folder_path)
                args.append(
                    f"--add-data={sub_path + os.pathsep + os.path.join('ConfDefinitions', rel_path)}"
                )

    if output_file_path is not None:
        if os.path.isdir(output_file_path):
            args.append(f"--distpath={output_file_path}")
        else:
            print(
                "The specified output file path does not exist or is not a directory."
            )
            return -1

    PyInstaller.__main__.run(args)


#
# main script function
#
def main():
    parser = argparse.ArgumentParser(
        description="Create executables for ConfigEditor, ReadUefiVarsToConfVarList, and WriteConfVarListToUefiVars"
    )

    parser.add_argument(
        "script",
        choices=[
            "ConfigEditor",
            "ReadUefiVarsToConfVarList",
            "WriteConfVarListToUefiVars",
            "all",
        ],
        help="Specify which script to build or 'all' to build all scripts",
    )
    parser.add_argument(
        "--ConfFolderPath",
        dest="ConfFolderPath",
        help="Absolute path to all configuration definitions to be embedded, subfolder structure will be kept",
        default=None,
    )
    parser.add_argument(
        "--OutputFilePath",
        dest="OutputFilePath",
        help="Absolute path to output executable, default will be ./dist/",
        default=None,
    )

    options = parser.parse_args()

    base_dir = os.path.dirname(os.path.abspath(__file__))
    scripts = {
        "ConfigEditor": os.path.join(base_dir, "ConfigEditor.py"),
        "ReadUefiVarsToConfVarList": os.path.join(
            base_dir, "ReadUefiVarsToConfVarList.py"
        ),
        "WriteConfVarListToUefiVars": os.path.join(
            base_dir, "WriteConfVarListToUefiVars.py"
        ),
    }

    for key, value in scripts.items():
        print(f"Script {key}: {value}")  # Debugging line

    if options.script == "all":
        for script_name, script_path in scripts.items():
            build_executable(
                script_name, script_path, options.ConfFolderPath, options.OutputFilePath
            )
    else:
        build_executable(
            options.script,
            scripts[options.script],
            options.ConfFolderPath,
            options.OutputFilePath,
        )

    return 0


if __name__ == "__main__":
    # setup main console as logger
    logger = logging.getLogger("")
    logger.setLevel(logging.DEBUG)
    formatter = logging.Formatter("%(levelname)s - %(message)s")
    console = logging.StreamHandler()
    console.setLevel(logging.CRITICAL)
    console.setFormatter(formatter)
    logger.addHandler(console)

    # call main worker function
    retcode = main()

    if retcode != 0:
        logging.critical("Failed.  Return Code: %i" % retcode)
    # end logging
    logging.shutdown()
    sys.exit(retcode)
