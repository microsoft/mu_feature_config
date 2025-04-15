## @ GenNCCfgData.py
#
# Copyright(c) 2020, Intel Corporation. All rights reserved.<BR>
# Copyright(c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import sys
import re
from collections import OrderedDict
import base64

import xml.etree.ElementTree as ET
import WriteConfVarListToUefiVars as uefi_var_write             # noqa: E402
from CommonUtility import bytes_to_value
from VariableList import (
    Schema,
    IntValueFormat,
    FloatValueFormat,
    BoolFormat,
    EnumFormat,
    StructFormat,
    ArrayFormat,
    vlist_to_binary,
    read_vlist_from_buffer,
    uefi_variables_to_knobs,
    write_csv,
    read_csv,
    create_vlist_buffer,
    get_delta_vlist
)


class CGenNCCfgData:
    def __init__(self, file_name):
        self.load_xml(file_name)

    def initialize(self, file_name):
        self._cfg_page = {"root": {"title": "", "child": []}}
        # name of this page is filename minus suffix, get rid of full path if present
        match_res = re.search(r".*[\\/]", file_name)
        if match_res is not None:
            file_name = file_name[match_res.end():]
        self._cur_page = file_name[:-4]
        self._cfg_page["root"]["child"].append(
            {self._cur_page: {"title": self._cur_page, "child": []}}
        )

    def get_last_error(self):
        return ""

    def get_cfg_list(self, page_id=None):
        if page_id is None:
            # return full list
            return self.knob_shim
        else:
            # build a new list for items under a page ID
            cfgs = [
                i
                for i in self.knob_shim
                if (".".join(i["path"].split(".")[:2]) == page_id)
            ]
            return cfgs

    def get_cfg_page(self):
        return self._cfg_page

    def get_cfg_item_length(self, item):
        return item["inst"].format.size_in_bytes()

    def get_cfg_item_value(self, item, array=False):
        length = item["inst"].format.size_in_bytes()
        return self.get_value(item, length, array)

    def reformat_value_str(self, value_str, bit_length, old_value=None, item=None):
        if item is None:
            raise Exception("Cannot accept item being None for xml parser!!!")
        obj = item["inst"].format.string_to_object(value_str)
        new_value = item["inst"].format.object_to_string(obj)
        return new_value

    def get_value(self, item, bit_length, array=True, value_str=None):
        if value_str is None:
            value_str = item["value"].strip()
        data_inst = item["inst"]
        if len(value_str) == 0:
            return 0
        obj = data_inst.format.string_to_object(value_str)
        bvalue = data_inst.format.object_to_binary(obj)
        if array:
            return bvalue
        else:
            return bytes_to_value(bvalue)

    def set_item_value(self, value_str, item):
        if item is None:
            raise Exception("Cannot accept item being None for xml parser!!!")
        subknob = item["inst"]
        subknob.value = subknob.format.string_to_object(value_str)
        new_value = subknob.format.object_to_string(subknob.value)
        return new_value

    def get_cfg_item_options(self, item):
        tmp_list = []
        if item["type"].upper() == "ENUM_KNOB":
            if type(item["inst"].format) is not EnumFormat:
                raise Exception("The item is malformed!!!")
            tmp_list = [i.pretty_name for i in item["inst"].format.values]
        elif item["type"].upper() == "BOOL_KNOB":
            if type(item["inst"].format) is not BoolFormat:
                raise Exception("The item is malformed!!!")
            tmp_list = ["true", "false"]
        return tmp_list

    def get_page_title(self, page_id, top=None):
        if top is None:
            top = self.get_cfg_page()["root"]
        for node in top["child"]:
            page_key = next(iter(node))
            if page_id == page_key:
                return node[page_key]["title"]
            else:
                result = self.get_page_title(page_id, node[page_key])
                if result is not None:
                    return result
        return None

    def get_item_by_path(self, path):
        for each in self.knob_shim:
            path_list = path.split(".")
            namespace = path_list[0]
            var_path = ".".join(path_list[1:])
            if (
                each["inst"].knob.namespace == namespace
                and each["inst"].name == var_path
                and each["inst"].leaf is True
            ):
                return each
        return None

    def add_cfg_page(self, child, parent, title=""):
        def _add_cfg_page(cfg_page, child, parent):
            key = next(iter(cfg_page))
            if parent == key:
                cfg_page[key]["child"].append({child: {"title": title, "child": []}})
                return True
            else:
                result = False
                for each in cfg_page[key]["child"]:
                    if _add_cfg_page(each, child, parent):
                        result = True
                        break
                return result

        return _add_cfg_page(self._cfg_page, child, parent)

    def build_cfg_list(self):

        # Add the pages for each root knob
        for knob in self.schema.knobs:
            # try to use pretty name if it exists
            print_name = knob.pretty_name
            if print_name == '':
                print_name = knob.name
            if not self.add_cfg_page(
                child=".".join([knob.namespace, knob.name]),
                parent=knob.namespace,
                title=print_name,
            ):
                # The parent page may not exist yet
                self.add_cfg_page(child=knob.namespace, parent=self._cur_page, title=knob.namespace)
                if not self.add_cfg_page(
                    child=".".join([knob.namespace, knob.name]),
                    parent=knob.namespace,
                    title=print_name,
                ):
                    raise Exception("Failed to add page for knob %s" % knob.name)

        ret_list = []
        # below is a shim layer that connects the UI and data structure
        for idx, data in enumerate(self.schema.subknobs):
            itype = type(data.format)
            name = data.pretty_name
            if name == '':
                name = data.name
            ord_dict = OrderedDict()
            if itype is IntValueFormat:
                ord_dict["type"] = "INTEGER_KNOB"
            elif itype is FloatValueFormat:
                ord_dict["type"] = "FLOAT_KNOB"
            elif itype is BoolFormat:
                ord_dict["type"] = "BOOL_KNOB"
            elif itype is EnumFormat:
                ord_dict["type"] = "ENUM_KNOB"
            elif itype is StructFormat:
                ord_dict["type"] = "STRUCT_KNOB"
            elif itype is ArrayFormat:
                ord_dict["type"] = "ARRAY_KNOB"
            else:
                raise Exception("Unrecognized data format %s!" % str(itype))
            ord_dict["inst"] = data
            ord_dict["order"] = idx
            ord_dict["name"] = name
            ord_dict["cname"] = data.name
            ord_dict["value"] = data.format.object_to_string(data.value)
            ord_dict["path"] = ".".join([data.knob.namespace, data.name])
            ord_dict["help"] = data.help
            ret_list.append(ord_dict)

        return ret_list

    def sync_shim_and_schema(self):
        for shim in self.knob_shim:
            data = shim["inst"]
            shim["value"] = data.format.object_to_string(data.value)

    def generate_delta_svd_from_bin(self, old_data, new_data):
        # return list of UEFI vars in buffers that have changed data and list of names of vars
        return get_delta_vlist(self.schema)

    def get_var_list_for_instance(self, var_list):
        loaded_vars = read_vlist_from_buffer(self.generate_binary_array(True))
        actual_var_list = []
        for var in var_list:
            for loaded_var in loaded_vars:
                if loaded_var.guid == var.guid and loaded_var.name == var.name:
                    actual_var_list.append(var)
                    break
        return actual_var_list

    def iterate_each_setting(self, resultfile, handler):
        xmlstring = ""
        found = False
        a = open(resultfile, "r")

        # find the start of the xml string and then copy all lines to xmlstring variable
        for line in a.readlines():
            if found:
                xmlstring += line
            else:
                if line.lstrip().startswith("<?xml"):
                    xmlstring = line
                    found = True
        a.close()

        if (len(xmlstring) == 0) or (not found):
            print("Result XML not found")
            return None

        # make an element tree from xml string
        r = None
        root = ET.fromstring(xmlstring)

        # Process Settings produced by SettingsXMLLib.py
        for e in root.findall(
            "./{urn:UefiSettings-Schema}Settings/{urn:UefiSettings-Schema}Setting"
        ):
            i = e.find("{urn:UefiSettings-Schema}Id")
            r = e.find("{urn:UefiSettings-Schema}Value")
            handler(i.text, r.text)

        # Process SettingsCurrent from ConfApp output (from DFCI Libs)
        for e in root.findall("./Settings/SettingCurrent"):
            i = e.find("Id")
            r = e.find("Value")
            handler(i.text, r.text)

    def load_from_svd(self, path):
        def handler(id, value):
            # ignore YAML entries
            if id is not None:
                # this is an xml section
                base64_val = value.strip()
                bin_data = base64.b64decode(base64_val)
                var_list = read_vlist_from_buffer(bin_data)
                actual_var_list = self.get_var_list_for_instance(var_list)
                uefi_variables_to_knobs(self.schema, actual_var_list)
                self.sync_shim_and_schema()

        self.iterate_each_setting(path, handler)

    def load_default_from_bin(self, bin_data, is_variable_list_format):
        var_list = read_vlist_from_buffer(bin_data)

        # ensure that read in variables are part of schema, otherwise ignore them
        actual_var_list = self.get_var_list_for_instance(var_list)

        uefi_variables_to_knobs(self.schema, actual_var_list)
        self.sync_shim_and_schema()

    def get_var_by_index(self, index):
        vlist = self.generate_binary_array(True)
        variables = read_vlist_from_buffer(vlist)

        if (len(variables) <= index):
            return None, None

        return create_vlist_buffer(variables[index]), variables[index].name

    def generate_binary_array(self, is_variable_list_format):
        return vlist_to_binary(self.schema)

    def generate_delta_binary_array(self):
        bin = b''
        list = get_delta_vlist(self.schema)[1]

        for var in list:
            bin += var

        return bin

    def generate_binary(self, bin_file_name):
        bin_file = open(bin_file_name, "wb")
        bin_file.write(self.generate_binary_array(True))
        bin_file.close()
        return 0

    def override_default_value(self, csv_file):
        updated_knobs = read_csv(self.schema, csv_file)
        self.sync_shim_and_schema()
        return updated_knobs

    def generate_delta_file_from_bin(self, delta_file, old_data, new_data, full, subknobs=False):
        self.load_default_from_bin(new_data, True)
        # by default, create smaller csv files with only the full knobs. If more detail is required
        # this default can be changed to include the subknobs
        write_csv(self.schema, delta_file, full, subknobs=subknobs)
        return 0

    def generate_csv_file(self, delta_file, bin_file, bin_file2, full=False):
        fd = open(bin_file, "rb")
        new_data = bytearray(fd.read())
        fd.close()

        if bin_file2 == "":
            old_data = self.generate_binary_array(True)
        else:
            old_data = new_data
            fd = open(bin_file2, "rb")
            new_data = bytearray(fd.read())
            fd.close()

        return self.generate_delta_file_from_bin(delta_file, old_data, new_data, full, False)

    def load_xml(self, cfg_file):
        self.initialize(cfg_file)
        self.schema = Schema.load(cfg_file)

        # Assign all values to their defaults
        for knob in self.schema.knobs:
            knob.value = knob.default

        self.knob_shim = self.build_cfg_list()
        return 0

    def delete_all_variables(self, config_xml_path):
        schema = Schema.load(config_xml_path)
        for knob in schema.knobs:
            rc = uefi_var_write.delete_var_by_guid_name(knob.name, knob.namespace)
            if rc == 0:
                print(f"Error returned from SetUefiVar: {rc}")
            else:
                print(f"{knob.name} variable is deleted from system")

    def modify_variables(self, variables, new_values, output_file="data.vl"):
        if len(variables) != len(new_values):
            print("Error: The number of variables and new values must match.")
            return

        for knob in self.schema.knobs:
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
                    elif isinstance(new_value, str) and new_value.startswith("[") and new_value.endswith("]"):
                        new_knob_value[variable_name] = [int(x) for x in new_value[1:-1].split(",")]
                    else:
                        new_knob_value[variable_name] = str(new_value)  # Treat as a string

                    # Update the variable in the schema
                    knob.value = new_knob_value
                    print(f"Updated {variable_name} to {knob.value[variable_name]}")

        self.generate_binary(output_file)
        uefi_var_write.set_variable_from_file(output_file)


def usage():
    print(
        "Usage:\n"
        "    python GenNCCfgData.py GENBIN <XmlFile[;CsvFile]> <BinOutFile>\n"
        "    python GenNCCfgData.py GENCSV <XmlFile;BinFile[;BinFile2]> <CsvOutFile>\n"
        "    python GenNCCfgData.py MODCONFIG <xml_file> <variables> <values> [output_file]\n"
        "    python GenNCCfgData.py DELVAR <xml_file>"
    )


def main():
    # Parse the options and args manually
    argc = len(sys.argv)
    if argc < 3:
        usage()
        return 1

    command = sys.argv[1].upper()

    # Handle MODCONFIG and DELVAR commands first
    if command == "MODCONFIG":
        if argc < 5:
            print("Usage: python GenNCCfgData.py MODCONFIG <xml_file> <variables> <values> [output_file]")
            return 1

        xml_file = sys.argv[2]
        variables = sys.argv[3].split(";")  # Split variables by semicolon
        values = sys.argv[4].split(";")    # Split values by semicolon
        output_file = sys.argv[5] if argc > 5 else "data.vl"

        if len(variables) != len(values):
            print("Error: The number of variables and values must match.")
            return 1

        # Call the modify_variables function
        gen_cfg_data = CGenNCCfgData(xml_file)
        gen_cfg_data.modify_variables(variables, values, output_file)
        return 0

    elif command == "DELVAR":
        if argc < 3:
            print("Usage: python GenNCCfgData.py DELVAR <xml_file>")
            return 1

        xml_file = sys.argv[2]
        gen_cfg_data = CGenNCCfgData(xml_file)
        gen_cfg_data.delete_all_variables(xml_file)
        return 0

    elif command == "GENBIN" or command == "GENCSV":
        if argc < 4 or argc > 5:
            usage()
            return 1

        out_file = sys.argv[3]

        file_list = sys.argv[2].split(";")
        if len(file_list) >= 2:
            xml_file = file_list[0]
            csv_file = file_list[1]
        elif len(file_list) == 1:
            xml_file = file_list[0]
            csv_file = ""
        else:
            raise Exception("ERROR: Invalid parameter '%s' !" % sys.argv[2])

        cfg_bin_file = ""
        cfg_bin_file2 = ""
        if csv_file:
            if command == "GENCSV":
                cfg_bin_file = csv_file
                csv_file = ""
                if len(file_list) >= 3:
                    cfg_bin_file2 = file_list[2]

        gen_cfg_data = CGenNCCfgData(xml_file)

        if csv_file:
            gen_cfg_data.override_default_value(csv_file)

        if command == "GENBIN":
            if len(file_list) == 3:
                old_data = gen_cfg_data.generate_binary_array(True)
                fi = open(file_list[2], "rb")
                new_data = bytearray(fi.read())
                fi.close()
                if len(new_data) != len(old_data):
                    raise Exception(
                        "Binary file '%s' length does not match, ignored !" % file_list[2]
                    )
                else:
                    gen_cfg_data.load_default_from_bin(new_data, True)
                    gen_cfg_data.override_default_value(csv_file)

            gen_cfg_data.generate_binary(out_file)

        elif command == "GENCSV":
            gen_cfg_data.generate_csv_file(out_file, cfg_bin_file, cfg_bin_file2)

    else:
        raise Exception("Unsupported command '%s' !" % command)

    return 0


if __name__ == "__main__":
    sys.exit(main())
