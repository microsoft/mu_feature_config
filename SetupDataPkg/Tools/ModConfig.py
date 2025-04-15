## @ ModConfig.py
#
# Provide cmd line interface to modify/delete config variables in the system.
#
# Copyright(c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import argparse
import WriteConfVarListToUefiVars as uefi_var_write             # noqa: E402
from GenNCCfgData import CGenNCCfgData                          # noqa: E402
from VariableList import Schema                                 # noqa: E402


def delete_all_variables(config_xml_path):
    schema = Schema.load(config_xml_path)
    for knob in schema.knobs:
        rc = uefi_var_write.delete_var_by_guid_name(knob.name, knob.namespace)
        if rc == 0:
            print(f"Error returned from SetUefiVar: {rc}")
        else:
            print(f"{knob.name} variable is deleted from system")


def modify_variables(config_xml_path, variables, new_values, output_file):
    if len(variables) != len(new_values):
        print("Error: The number of variables and new values must match.")
        return

    NCCfData = CGenNCCfgData(config_xml_path)
    for knob in NCCfData.schema.knobs:
        for i, variable_name in enumerate(variables):
            if variable_name in knob.value.keys():
                print(f"Orig {variable_name} is {knob.value[variable_name]}")

                new_knob_value = knob.value
                new_value = new_values[i]
                # Check if the new value is a hexadecimal string or a decimal string
                if isinstance(new_value, str) and new_value.startswith("0x"):
                    new_knob_value[variable_name] = int(new_value, 16)  # Convert hex string to int
                elif new_value.isdigit():
                    new_knob_value[variable_name] = int(new_value)  # Convert decimal string to int
                else:
                    new_knob_value[variable_name] = str(new_value)  # Treat as a string

                # Update the variable in the schema
                knob.value = new_knob_value
                print(f"Updated {variable_name} to {knob.value[variable_name]}")

    NCCfData.generate_binary(output_file)
    uefi_var_write.set_variable_from_file(output_file)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ModConfig.py")
    parser.add_argument(
        "-d",
        "--delete",
        action="store_true",
        help="Delete all config variables from system",
    )
    parser.add_argument(
        "-c",
        "--config",
        type=str,
        help="Path to the config file",
    )
    parser.add_argument(
        "-m",
        "--modify",
        action="store_true",
        help="Modify a variable in the config file",
    )
    parser.add_argument(
        "-v",
        "--variable",
        type=str,
        nargs="+",
        help="Names of the variables to modify (space-separated list)",
    )
    parser.add_argument(
        "-n",
        "--new_value",
        type=str,
        nargs="+",
        help="New values for the variables (space-separated list)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default="data.vl",
        help="Output vl file name",
    )

    # Call corresponding function based on the command line arguments
    args = parser.parse_args()
    if args.delete:
        if args.config:
            delete_all_variables(args.config)
        else:
            print("Please provide a config file to delete variables")
    elif args.modify:
        if args.config and args.variable and args.new_value:
            modify_variables(args.config, args.variable, args.new_value, args.output)
        else:
            print("Please provide a config file, variables, and new values to modify")
    else:
        print("Please provide a valid command")
