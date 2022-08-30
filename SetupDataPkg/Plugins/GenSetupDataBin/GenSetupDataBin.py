##
# This plugin generates setup data binary blobs
# from platform supplied YAML and XML configurations.
#
# Copyright (c) Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import logging
import os
import shutil
import xml.etree.ElementTree
from edk2toolext.environment.plugintypes.uefi_build_plugin import IUefiBuildPlugin
from edk2toollib.utility_functions import RunPythonScript


class GenSetupDataBin(IUefiBuildPlugin):

    # Prettyprint formatter from https://stackoverflow.com/questions/749796/pretty-printing-xml-in-python
    @staticmethod
    def indent(elem, level=0):
        i = "\n" + level * "  "
        if len(elem):
            if not elem.text or not elem.text.strip():
                elem.text = i + "  "
            if not elem.tail or not elem.tail.strip():
                elem.tail = i
            for elem in elem:
                GenSetupDataBin.indent(elem, level + 1)
            if not elem.tail or not elem.tail.strip():
                elem.tail = i
        else:
            if level and (not elem.tail or not elem.tail.strip()):
                elem.tail = i

    def generate_profile(self, thebuilder, dlt_filename, csv_filename, idx):
        # Set up a playground
        op_dir = thebuilder.mws.join(thebuilder.ws, thebuilder.env.GetValue("BUILD_OUTPUT_BASE"), "ConfPolicy")
        if not os.path.isdir(op_dir):
            os.makedirs(op_dir)
        cmd = thebuilder.mws.join(thebuilder.ws, "SetupDataPkg", "Tools", "GenCfgData.py")

        params = ["GENBIN"]

        conf_file = thebuilder.env.GetValue("YAML_CONF_FILE")
        found_yaml_conf = False
        yaml_filename = None
        if conf_file is None:
            logging.info("YAML generic profile file not specified")
        else:
            found_yaml_conf = True
            if not os.path.isfile(conf_file):
                return -1

            # Can also add the dlt file application step if supplied
            if dlt_filename is not None:
                if not os.path.isfile(dlt_filename):
                    return -1
                conf_file = conf_file + ";" + dlt_filename
            params.append(conf_file)

            # Should be all setup, generate bin now...
            yaml_filename = os.path.join(op_dir, "YAMLPolicyVarBin_" + str(idx) + ".bin")
            params.append(yaml_filename)
            ret = RunPythonScript(cmd, " ".join(params))
            if ret != 0:
                return ret

        # Now generate XML config
        cmd = thebuilder.mws.join(thebuilder.ws, "SetupDataPkg", "Tools", "GenNCCfgData.py")
        params = ["GENBIN"]

        conf_file = thebuilder.env.GetValue("XML_CONF_FILE")
        found_xml_conf = False
        xml_filename = None
        if conf_file is None:
            logging.warn("XML generic profile file not specified")
            if not found_yaml_conf:
                logging.error("Did not find any profile config files!")
                return -1
        else:
            found_xml_conf = True
            if not os.path.isfile(conf_file):
                return -1

            # Can also add the csv file application step if supplied
            if csv_filename is not None:
                if not os.path.isfile(csv_filename):
                    return -1
                conf_file = conf_file + ";" + csv_filename
            params.append(conf_file)

            # Should be all setup, generate bin now...
            xml_filename = os.path.join(op_dir, "XMLPolicyVarBin_" + str(idx) + ".bin")
            params.append(xml_filename)
            ret = RunPythonScript(cmd, " ".join(params))
            if ret != 0:
                return ret

        # Combine into single bin file
        combined_bin = os.path.join(op_dir, "ConfPolicyVarBin_" + str(idx) + ".bin")
        with open(combined_bin, "wb") as bin_out:
            if found_yaml_conf and found_xml_conf:
                with open(yaml_filename, "rb") as yml_file, open(xml_filename, "rb") as xml_file:
                    yaml_bytes = yml_file.read()
                    xml_bytes = xml_file.read()
                    bin_out.write(yaml_bytes + xml_bytes)
            elif found_yaml_conf:
                with open(yaml_filename, "rb") as yml_file:
                    yaml_bytes = yml_file.read()
                    bin_out.write(yaml_bytes)
            else:
                # xml only
                with open(xml_filename, "rb") as xml_file:
                    xml_bytes = xml_file.read()
                    bin_out.write(xml_bytes)

    # Attempt to run GenCfgData to generate setup data binary blob, output will be placed at
    # "MSFT_PLATFORM_PACKAGE"/BuiltInVars.xml
    #
    # Consumes build environment variables: "BUILD_OUTPUT_BASE", "YAML_CONF_FILE", "XML_CONF_FILE",
    # "DELTA_CONF_POLICY" (optional), "CSV_CONF_POLICY" (optional), "CONF_POLICY_GUID_REGISTRY", and
    # "MSFT_PLATFORM_PACKAGE"
    def do_pre_build(self, thebuilder):
        # Generate Generic Profile
        self.generate_profile(thebuilder, None, None, 0)

        # Build other profiles, if present
        delta_conf = thebuilder.env.GetValue("DELTA_CONF_POLICY")
        csv_conf = thebuilder.env.GetValue("CSV_CONF_POLICY")

        if delta_conf is not None and csv_conf is not None:
            delta_conf = delta_conf.split(";")
            csv_conf = csv_conf.split(";")
            # Validate we have the same number of delta and csv files, as required
            if len(delta_conf) != len(csv_conf):
                logging.error("Differing number of Delta and CSV files provided for profiles, they must be the same.")
                return -1

            for idx in range(len(delta_conf)):
                # Validate the name of the delta file and csv file match, to mitigate human error in placement
                if delta_conf[idx] != csv_conf[idx]:
                    logging.error("Delta and CSV files do not have the same name, possible misordering of list.")
                    return -1

                # Generate the profile
                self.generate_profile(thebuilder, delta_conf[idx], csv_conf[idx], idx + 1)
        elif delta_conf is not None:
            delta_conf = delta_conf.split(";")

            for idx in range(len(delta_conf)):
                # Generate the profile
                self.generate_profile(thebuilder, delta_conf[idx], None, idx + 1)
        elif csv_conf is not None:
            csv_conf = csv_conf.split(";")

            for idx in range(len(csv_conf)):
                # Generate the profile
                self.generate_profile(thebuilder, None, csv_conf[idx], idx + 1)
        else:
            logging.warn("Either DELTA_CONF_POLICY or CSV_CONF_POLICY or both not set. Only generic profile generated.")

        thebuilder.env.SetValue("BLD_*_CONF_BIN_FILE", "ConfPolicyVarBin_0.bin", "Plugin generated")
        return 0

    # Attempt to run GenCfgData to generate setup data binary blob, output will be placed at
    # "MSFT_PLATFORM_PACKAGE"/BuiltInVars.xml
    #
    # Consumes build environment variables: "MSFT_PLATFORM_PACKAGE"
    def do_post_build(self, thebuilder):
        if thebuilder.env.GetValue("MSFT_PLATFORM_PACKAGE") is not None:
            os.remove(thebuilder.mws.join(thebuilder.ws, thebuilder.env.GetValue("MSFT_PLATFORM_PACKAGE"),
                                          "BuiltInVars.xml"))
            # OSDDEBUG remove all partial bin files, after testing
        return 0
