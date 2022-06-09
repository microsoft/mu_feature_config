##
# This plugin generates setup data header files
# from platform supplied YAML configurations.
#
# Copyright (c) Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import logging
import os
from edk2toolext.environment.plugintypes.uefi_build_plugin import IUefiBuildPlugin
from edk2toollib.utility_functions import RunPythonScript


class UpdateSetupDataHdr(IUefiBuildPlugin):

    # Attempt to run GenCfgData to generate C header files
    #
    # Consumes build environment variables: "BUILD_OUTPUT_BASE", "YAML_CONF_FILE",
    # "CONF_DATA_STRUCT_FOLDER" and "UPDATE_CONF_HDR"
    def do_pre_build(self, thebuilder):
        need_update = thebuilder.env.GetValue("UPDATE_CONF_HDR")
        if need_update is None or need_update.upper() != "TRUE":
            logging.info("YAML file not specified, system might not work as expected!!!")
            return 0

        final_dir = thebuilder.env.GetValue("CONF_DATA_STRUCT_FOLDER")
        if final_dir is None or not os.path.isdir(final_dir):
            logging.error("Invalid CONF_DATA_STRUCT_FOLDER configured!!!")
            return -1

        # Set up a playground first
        op_dir = thebuilder.mws.join(thebuilder.ws, thebuilder.env.GetValue("BUILD_OUTPUT_BASE"), "ConfPolicy")
        if not os.path.isdir(op_dir):
            os.makedirs(op_dir)

        # Add GENPKL routine here
        cmd = thebuilder.mws.join(thebuilder.ws, "SetupDataPkg", "Tools", "GenCfgData.py")
        params = ["GENPKL"]

        conf_file = thebuilder.env.GetValue("YAML_CONF_FILE")
        if conf_file is None:
            logging.warn("YAML file not specified, system might not work as expected!!!")
            return 0
        if not os.path.isfile(conf_file):
            logging.error("YAML file specified is not found!!!")
            return -1
        params.append(conf_file)

        # Should be all setup, generate bin now...
        op_pkl = os.path.join(op_dir, "temp.pkl")
        params.append(op_pkl)
        ret = RunPythonScript(cmd, " ".join(params))
        if ret != 0:
            return ret

        # Now that we have the PKL file, output the header files
        params = ["GENHDR"]

        params.append(op_pkl)

        params.append("'ConfigDataStruct.h;ConfigDataCommonStruct.h'")

        ret = RunPythonScript(cmd, " ".join(params), workingdir=final_dir)
        return ret
