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
    # Consumes build environment variables: "CONF_AUTOGEN_INCLUDE_PATH", "MU_SCHEMA_DIR",
    # "MU_SCHEMA_FILE_NAME", "CONF_PROFILE_PATHS" and "CONF_PROFILE_NAMES"
    def do_pre_build(self, thebuilder):
        default_generated_path = thebuilder.edk2path.GetAbsolutePathOnThisSystemFromEdk2RelativePath(
            "SetupDataPkg", "Test", "Include"
        )

        # we can have a semicolon delimited list of include paths to generate to
        dirs = thebuilder.env.GetValue("CONF_AUTOGEN_INCLUDE_PATH", default_generated_path).split(";")
        final_dirs = []

        # Add Generated dir
        for directory in dirs:
            gen_dir = os.path.join(directory, "Generated")
            final_dirs.append(gen_dir)

            if not os.path.isdir(gen_dir):
                os.makedirs(gen_dir)

        # Add generate routine here
        cmd = thebuilder.edk2path.GetAbsolutePathOnThisSystemFromEdk2RelativePath(
            "SetupDataPkg", "Tools", "KnobService.py"
        )

        # MU_SCHEMA_DIR and MU_SCHEMA_FILE_NAME should be set by individual platforms
        # When running the CI, the configdata_ci scope will define MU_SCHEMA_DIR to the path of the
        # appropriate testschema.xml
        schema_dir = thebuilder.env.GetValue("MU_SCHEMA_DIR")
        if schema_dir is None:
            logging.error("MU_SCHEMA_DIR not set")
            return -1

        # This may be a semicolon delimited string
        schema_file_names = thebuilder.env.GetValue("MU_SCHEMA_FILE_NAME", "testschema.xml").split(";")
        schema_files = []

        for name in schema_file_names:
            file = thebuilder.edk2path.GetAbsolutePathOnThisSystemFromEdk2RelativePath(schema_dir, name)
            schema_files.append(file)

            if file is None:
                logging.error(f"XML schema file \"{schema_dir}/{name}\" specified is not found!!!")
                return -1

        # this is a semicolon delimited list of space separated lists of paths to CSV files describing the profiles for
        # this platform. It is allowed to be empty if there are no profiles.
        # e.g.: "CONF_PATH_1 CONF_PATH_2 CONF_PATH3;CONF_PATH4 CONF_PATH5 CONF_PATH6;CONF_PATH7 CONF_PATH8 CONF_PATH9"
        profile_path_list = thebuilder.env.GetValue("CONF_PROFILE_PATHS", "").split(";")

        # this is a semicolon delimited list of comma separated 2 character profile names to pair with CSV files
        # identifying the profiles. This field is optional.
        profile_names = thebuilder.env.GetValue("CONF_PROFILE_NAMES", "").split(";")
        profile_ids = thebuilder.env.GetValue("CONF_PROFILE_IDS", "").split(";")

        if len(schema_files) != len(final_dirs):
            logging.error("Differing number of items in CONF_AUTOGEN_INCLUDE_PATH and MU_SCHEMA_FILE_NAME!\
                           They must be the same")
            return -1

        for i in range(len(schema_files)):
            params = ["generateheader_efi"]

            params.append(schema_files[i])

            params.append("ConfigClientGenerated.h")
            params.append("ConfigServiceGenerated.h")
            params.append("ConfigDataGenerated.h")

            if len(profile_path_list) > i and profile_path_list[i] != "":
                params.append("ConfigProfilesGenerated.h")
                params.append(profile_path_list[i])

                if len(profile_names) > i and profile_names[i] != "":
                    params.append("-pn")
                    params.append(profile_names[i])
                if len(profile_ids) > i and profile_ids[i] != "":
                    params.append("-pid")
                    params.append(profile_ids[i])

            ret = RunPythonScript(cmd, " ".join(params), workingdir=final_dirs[i])
            if ret != 0:
                return ret
        return 0
