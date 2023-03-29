# @file
#
# Set a dmpstore formatted set of variables to NVRAM
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
from SettingSupport.UefiVariablesSupportLib import UefiVariable
from VariableList import Schema, UEFIVariable, create_vlist_buffer


def option_parser():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "-x",
        "--xml",
        dest="configuration_file",
        required=False,
        type=str,
        help="""Specify the input setting file""",
    )

    parser.add_argument(
        "-o",
        "--output",
        dest="output_file",
        required=False,
        type=str,
        default="output.vl",
        help="""Specify the output file path and name, in vl format""",
    )

    arguments = parser.parse_args()

    if arguments.configuration_file is not None and not os.path.isfile(arguments.configuration_file):
        print("Invalid input file: %s" % arguments.configuration_file)
        sys.exit(1)

    return arguments


#
# Create an unpack statement formatted to address the variable length
# parameters in the var store (Data and Unicode Name)
def read_variable_into_variable_list(uefivar, name, namespace):
    (rc, var, error_string) = uefivar.GetUefiVar(name, namespace)
    if rc != 0:
        logging.error(f"Error returned from GetUefiVar: {rc} on Name: {name}, Guid: {namespace}")
        return b''

    variable = UEFIVariable(name, namespace, var)
    b_array = create_vlist_buffer(variable)
    return b_array


#
# main script function
#
def main():
    arguments = option_parser()

    UefiVar = UefiVariable()

    # read the entire file
    with open(arguments.output_file, "wb") as file:
        ret = b''
        if arguments.configuration_file is None:
            # Read all the variables
            (rc, efi_var_names, error_string) = UefiVar.GetUefiAllVarNames()
            if rc != 0:
                logging.error(f"Error returned from GetUefiAllVarNames: {rc}")

            offset = 0
            while offset < len(efi_var_names):
                (next_offset,) = struct.unpack_from("<I", efi_var_names[offset:])
                if next_offset == 0:
                    # This is the end... But we still need to go through the last loop
                    next_offset = len(efi_var_names) - offset
                namespace = uuid.UUID(bytes_le=efi_var_names[offset + 0x04: offset + 0x14])
                name = efi_var_names[offset + 0x14: offset + next_offset].decode('utf16')
                ret += read_variable_into_variable_list(UefiVar, name, namespace)
                offset += next_offset
        else:
            # Read the variables for each config knobs
            schema = Schema.load(arguments.configuration_file)
            for knob in schema.knobs:
                ret += read_variable_into_variable_list(UefiVar, knob.name, knob.namespace)

        if len(ret) != 0:
            file.write(ret)
        else:
            logging.error("No variables found!!!")
            return 1
    return 0


if __name__ == "__main__":
    # setup main console as logger
    logger = logging.getLogger("")
    logger.setLevel(logging.INFO)
    formatter = logging.Formatter("%(levelname)s - %(message)s")
    console = logging.StreamHandler()
    console.setLevel(logging.CRITICAL)

    # check the privilege level and report error
    if not ctypes.windll.shell32.IsUserAnAdmin():
        print("Administrator privilege required. Please launch from an Administrator privilege level.")
        sys.exit(1)

    # call main worker function
    retcode = main()

    logging.shutdown()
    sys.exit(retcode)
