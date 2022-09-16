##
# This plugin generates setup data binary blobs
# from platform supplied YAML configurations.
#
# Copyright (c) Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import logging
import os
from edk2toolext.environment.plugintypes.uefi_build_plugin import IUefiBuildPlugin
from edk2toollib.utility_functions import RunPythonScript


class GenSetupDataBin(IUefiBuildPlugin):

    def generate_profile(self, thebuilder, dlt_filename, csv_filename, idx):
        # Set up a playground
        op_dir = thebuilder.mws.join(thebuilder.ws, thebuilder.env.GetValue("BUILD_OUTPUT_BASE"), "ConfPolicy")
        if not os.path.isdir(op_dir):
            os.makedirs(op_dir)
        cmd = thebuilder.mws.join(thebuilder.ws, "SetupDataPkg", "Tools", "GenCfgData.py")

        params = ["GENBIN"]

        conf_file = thebuilder.env.GetValue("YAML_CONF_FILE")
        yaml_filename = None
        if conf_file is None or not os.path.isfile(conf_file):
            logging.error("YAML generic profile file not specified")
            return -1

        # Can also add the dlt file application step if supplied
        if dlt_filename is not None:
            if not os.path.isfile(dlt_filename):
                return -1
            conf_file = '"' + conf_file + ";" + dlt_filename + '"'
        params.append(conf_file)

        # Should be all setup, generate bin now...
        yaml_filename = os.path.join(op_dir, "ConfPolicyVarBin_" + str(idx) + ".bin")
        params.append(yaml_filename)
        ret = RunPythonScript(cmd, " ".join(params))
        if ret != 0:
            return ret

        thebuilder.env.SetValue("BLD_*_CONF_BIN_FILE_" + str(idx), yaml_filename, "Plugin generated")

        return 0

    # Attempt to run GenCfgData to generate setup data binary blob, output will be placed at
    # ConfPolicyVarBin_*.bin
    #
    # Consumes build environment variables:
    # "BUILD_OUTPUT_BASE": root of build output
    # "YAML_CONF_FILE": absolute file path of a YAML configuration file
    # "DELTA_CONF_POLICY": semicolon delimited list of absolute file paths for YAML delta files to be built as
    #                      additional profiles. Only valid if YAML_CONF_FILE is populated and multiple profiles desired.
    def do_pre_build(self, thebuilder):
        # Generate Generic Profile
        ret = self.generate_profile(thebuilder, None, None, 0)

        if ret != 0:
            return ret

        # Build other profiles, if present
        delta_conf = thebuilder.env.GetValue("DELTA_CONF_POLICY")

        if delta_conf is not None:
            delta_conf = delta_conf.split(";")

            for idx in range(len(delta_conf)):
                # Generate the profile
                ret = self.generate_profile(thebuilder, delta_conf[idx], None, idx + 1)

                if ret != 0:
                    return ret
        else:
            logging.warn("DELTA_CONF_POLICY not set. Only generic profile generated.")

        return 0
