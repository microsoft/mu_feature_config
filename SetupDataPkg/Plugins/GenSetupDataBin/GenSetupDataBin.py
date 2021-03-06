##
# This plugin generates setup data binary blobs
# from platform supplied YAML configurations.
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

    # Attempt to run GenCfgData to generate setup data binary blob, output will be placed at
    # "MSFT_PLATFORM_PACKAGE"/BuiltInVars.xml
    #
    # Consumes build environment variables: "BUILD_OUTPUT_BASE", "YAML_CONF_FILE",
    # "DELTA_CONF_POLICY" (optional), "CONF_POLICY_GUID_REGISTRY" and "MSFT_PLATFORM_PACKAGE"
    def do_pre_build(self, thebuilder):
        # Set up a playground
        op_dir = thebuilder.mws.join(thebuilder.ws, thebuilder.env.GetValue("BUILD_OUTPUT_BASE"), "ConfPolicy")
        if not os.path.isdir(op_dir):
            os.makedirs(op_dir)

        # Add GENBIN routine here
        cmd = thebuilder.mws.join(thebuilder.ws, "SetupDataPkg", "Tools", "GenCfgData.py")
        params = ["GENBIN"]

        conf_file = thebuilder.env.GetValue("YAML_CONF_FILE")
        if conf_file is None:
            logging.warn("YAML file not specified, system might not work as expected!!!")
            return 0
        if not os.path.isfile(conf_file):
            return -1

        # Can also add the dlt file application step if supplied
        delta_conf = thebuilder.env.GetValue("DELTA_CONF_POLICY")
        if delta_conf is not None:
            if not os.path.isfile(delta_conf):
                return -1
            conf_file = ";".join(conf_file, delta_conf)
        params.append(conf_file)

        # Should be all setup, generate bin now...
        op_name = os.path.join(op_dir, "ConfPolicyVarBin.bin")
        params.append(op_name)
        ret = RunPythonScript(cmd, " ".join(params))
        if ret != 0:
            return ret
        thebuilder.env.SetValue("BLD_*_CONF_BIN_FILE", op_name, "Plugin generated")

        # Eventually generate a built in var xml
        op_xml = os.path.join(op_dir, "BuiltInVars.xml")
        with open(op_name, "rb") as in_file, open(op_xml, "wb") as out_file:
            bytes = in_file.read()
            comment = xml.etree.ElementTree.Comment(' === Auto-Generated === ')
            root = xml.etree.ElementTree.Element('BuiltInVariables')
            root.insert(0, comment)
            xml_var = xml.etree.ElementTree.SubElement(root, "Variable")
            var_name = xml.etree.ElementTree.SubElement(xml_var, "Name")
            var_name.text = 'CONF_POLICY_BLOB'
            var_guid = xml.etree.ElementTree.SubElement(xml_var, "GUID")
            var_guid.text = thebuilder.env.GetBuildValue("CONF_POLICY_GUID_REGISTRY")
            var_attr = xml.etree.ElementTree.SubElement(xml_var, "Attributes")
            # BS|NV
            var_attr.text = str(3)
            var_data = xml.etree.ElementTree.SubElement(xml_var, "Data")
            var_data.set('type', "hex")
            var_data.text = bytes.hex()
            GenSetupDataBin.indent(root)
            xml_str = xml.etree.ElementTree.tostring(root, encoding="utf-8", xml_declaration=True)
            out_file.write(xml_str)
        if thebuilder.env.GetValue("MSFT_PLATFORM_PACKAGE") is not None:
            shutil.copy2(op_xml, thebuilder.mws.join(thebuilder.ws, thebuilder.env.GetValue("MSFT_PLATFORM_PACKAGE")))
        return 0

    # Attempt to run GenCfgData to generate setup data binary blob, output will be placed at
    # "MSFT_PLATFORM_PACKAGE"/BuiltInVars.xml
    #
    # Consumes build environment variables: "MSFT_PLATFORM_PACKAGE"
    def do_post_build(self, thebuilder):
        if thebuilder.env.GetValue("MSFT_PLATFORM_PACKAGE") is not None:
            os.remove(thebuilder.mws.join(thebuilder.ws, thebuilder.env.GetValue("MSFT_PLATFORM_PACKAGE"),
                                          "BuiltInVars.xml"))
        return 0
