##
# This plugin generates config data header files
# from platform supplied XML configurations.
#
# Copyright (c) Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import logging
import os
from edk2toolext.environment.plugintypes.uefi_build_plugin import IUefiBuildPlugin
from edk2toollib.utility_functions import RunPythonScript


class UpdateConfigHdr(IUefiBuildPlugin):

    # Attempt to run GenCfgData to generate C header files
    #
    # Consumes build environment variables: "CONF_DATA_STRUCT_FOLDER", "MU_SCHEMA_DIR", and
    # "MU_SCHEMA_FILE_NAME"
    def do_pre_build(self, thebuilder):
        default_generated_path = thebuilder.mws.join(thebuilder.ws, "SetupDataPkg", "Test", "Include", "Generated")

        final_dir = thebuilder.env.GetValue("CONF_DATA_STRUCT_FOLDER", default_generated_path)

        if not os.path.isdir(final_dir):
            os.makedirs(final_dir)

        # Add generate routine here
        cmd = thebuilder.mws.join(thebuilder.ws, "SetupDataPkg", "Tools", "KnobService.py")

        # MU_SCHEMA_DIR and MU_SCHEMA_FILE_NAME should be set by individual platforms
        # When running the CI, the configdata_ci scope will define MU_SCHEMA_DIR to the path of the
        # appropriate testschema.xml
        schema_dir = thebuilder.env.GetValue("MU_SCHEMA_DIR")
        if schema_dir is None:
            logging.error("MU_SCHEMA_DIR not set")
            return -1

        schema_file_name = thebuilder.env.GetValue("MU_SCHEMA_FILE_NAME", "testschema.xml")

        schema_file = thebuilder.mws.join(schema_dir, schema_file_name)

        if not os.path.isfile(schema_file):
            logging.error(f"XML schema file \"{schema_file}\" specified is not found!!!")
            return -1

        params = ["generateheader_efi"]

        params.append(schema_file)

        params.append("ConfigClientGenerated.h")
        params.append("ConfigServiceGenerated.h")

        ret = RunPythonScript(cmd, " ".join(params), workingdir=final_dir)
        return ret
