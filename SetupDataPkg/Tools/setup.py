# @file
# Setup script to binarize ConfigEditor.
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import os
import sys
import argparse
import logging
import PyInstaller.__main__


#
# main script function
#
def main():

    parser = argparse.ArgumentParser(description='Create single executable for Configuration Editor')

    parser.add_argument("--ConfFolderPath", dest="ConfFolderPath", help="Absolute path to all configuration definitions "
                        "to be embedded, subfolder structure will be kept", default=None)
    parser.add_argument("--OutputFilePath", dest="OutputFilePath", help="Absolute path to output executable, default "
                        "will be./dist/", default=None)

    options = parser.parse_args()

    args = [os.path.join(os.path.dirname(os.path.realpath(__file__)), "ConfigEditor.py")]
    args.append("--clean")
    args.append("--onefile")

    if options.ConfFolderPath is not None:
        for subdir, _, files in os.walk(options.ConfFolderPath):
            for file in files:
                sub_path = os.path.join(subdir, file)
                rel_path = os.path.relpath(subdir, options.ConfFolderPath)
                args.append(f"--add-data={sub_path + os.pathsep + os.path.join('ConfDefinitions', rel_path)}")

    if options.OutputFilePath is not None:
        if os.path.isdir:
            args.append(f"--distpath={options.OutputFilePath}")
        else:
            return -1

    PyInstaller.__main__.run(args)

    return 0


if __name__ == '__main__':
    # setup main console as logger
    logger = logging.getLogger('')
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
