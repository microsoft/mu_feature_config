# @file
#
# Set a dumpbin formated set of variables to NVRAM
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent

import os
import sys
import logging
import argparse
import struct
import uuid
import ctypes
from SettingSupport.UefiVariablesSupportLib import UefiVariable  # noqa: E402

gEfiGlobalVariableGuid ="8BE4DF61-93CA-11D2-AA0D-00E098032B8C"

def option_parser():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "-guid",
        "--guid",
        dest="InputGuid",
        type=str,
        help="""Input Guid File""",
    )

    parser.add_argument(
        "-l",
        "--load",
        dest="setting_file",
        required=True,  
        type=str,
        default='""',
        help="""Specify the input setting file""",
    )

    arguments = parser.parse_args()

    if not os.path.isfile(arguments.setting_file):
        print("Invalid input file: %s" % arguments.setting_file)
        sys.exit(1)

    return arguments

#
# Create an unpack statement formatted to address the variable length
# paramters in the var store (Data and Unicode Name)
def create_unpackstatement(NameStringSize, DataBufferSize):

    #
    # Dmpstore Format taken from AppendSingleVariableToFile() in 
    # MU_BASECORE\ShellPkg\Library\UefiShellDebug1CommandsLib\DmpStore.c
    # NameSize
    # DataSize
    # Name[NameSize]
    # EFI_GUID
    # Attributes
    # Data[DataSize]
    # CRC32

    unpack_statement = "<II"
    unpack_statement = unpack_statement + f"{NameStringSize}s"
    unpack_statement = unpack_statement + "16s"
    unpack_statement = unpack_statement + "I"
    unpack_statement = unpack_statement + f"{DataBufferSize}s"
    unpack_statement = unpack_statement + "I"

    logging.debug(f"Created unpack statement {unpack_statement}")
    return unpack_statement

#
# Using the passed byte array, extract the variable data and
# write it into nvram
#
# Return the size of the un
#
def extract_single_var_from_file_and_write_nvram(var):
    if len(var) > 8:
        (NameSize, DataSize) = struct.unpack("<II", var[0:8])
        
        unpack_statement = create_unpackstatement(NameSize, DataSize)
        result = struct.unpack(unpack_statement, var[0: struct.calcsize(unpack_statement)])

        VarName = result[2].decode('utf16')
        Guid = uuid.UUID(bytes_le=result[3])
        Attributes = result[4]
        Data = result[5]
    
 
        logging.debug(f"Found Variable: {VarName} {Guid} {Attributes}")

        UefiVar = UefiVariable()
        (rc, err, error_string) = UefiVar.SetUefiVar(
            VarName,
            Guid,
            Data,
            Attributes,
        )
        if rc == 0:
            logging.debug(f"Error returned from SetUefiVar: {rc}")

        return struct.calcsize(unpack_statement)
    else:
        logging.critical("var buffer was too small to be a valid dmstore")

    return len(var)


#
# main script function
#
def main():
    arguments = option_parser()

    # read the entire file
    with open(arguments.setting_file, "rb") as file:
        var = file.read()

        # go through the entire file parsing each dmpstore variable
        start = 0;
        while len(var[start:]) != 0:
            start = start + extract_single_var_from_file_and_write_nvram(var[start:])

    return 0


if __name__ == "__main__":
    # setup main console as logger
    logger = logging.getLogger("")
    logger.setLevel(logging.INFO)
    formatter = logging.Formatter("%(levelname)s - %(message)s")
    console = logging.StreamHandler()
    console.setLevel(logging.CRITICAL)

    # check the priviledge level and report error 
    if not ctypes.windll.shell32.IsUserAnAdmin():
        print("Administrator priviledge required. Please launch from an Administrator priviledge level.")
        sys.exit(1)

    # call main worker function
    retcode = main()


    logging.shutdown()
    sys.exit(retcode)
