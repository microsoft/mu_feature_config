## @ GenNCCfgData.py
#
# Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import sys
from collections import OrderedDict
import base64
# import xmlschema

from SettingSupport.DFCI_SupportLib import DFCI_SupportLib      # noqa: E402
from CommonUtility import bytes_to_value
from VariableList import Schema, IntValueFormat, FloatValueFormat, BoolFormat, EnumFormat, StructFormat, ArrayFormat,\
    vlist_to_binary, read_vlist_from_buffer, uefi_variables_to_knobs, write_csv, read_csv

from GenCfgData import DFCI_SETTINGS_REQUEST_GUID

class CGenNCCfgData:

    def __init__(self):
        self.initialize()

    def initialize(self):
        self._cfg_page = {'root': {'title': '', 'child': []}}
        self._cur_page = 'Non-core'
        self._cfg_page['root']['child'].append({self._cur_page: {'title': self._cur_page, 'child': []}})

    def get_last_error(self):
        return ''

    def get_cfg_list(self, page_id=None):
        if page_id is None:
            # return full list
            return self.knob_shim
        else:
            # build a new list for items under a page ID
            cfgs = [i for i in self.knob_shim if ('.'.join(i['path'].split('.')[:2]) == page_id)]
            return cfgs

    def get_cfg_page(self):
        return self._cfg_page

    def get_cfg_item_length(self, item):
        return item['inst'].format.size_in_bytes()

    def get_cfg_item_value(self, item, array = False):
        length = item['inst'].format.size_in_bytes()
        return self.get_value(item, length, array)

    def reformat_value_str (self, value_str, bit_length, old_value = None, item=None):
        if item == None:
            raise Exception ("Cannot accept item being None for xml parser!!!")
        obj = item['inst'].format.string_to_object (value_str)
        new_value = item['inst'].format.object_to_string (obj)
        return new_value

    def get_value (self, item, bit_length, array = True, value_str = None):
        if value_str == None:
            value_str = item['value'].strip()
        data_inst = item['inst']
        if len(value_str) == 0:
            return 0
        obj = data_inst.format.string_to_object (value_str)
        bvalue = data_inst.format.object_to_binary (obj)
        if array:
            return  bvalue
        else:
            return  bytes_to_value (bvalue)

    def set_item_value (self, value_str, item):
        if item == None:
            raise Exception ("Cannot accept item being None for xml parser!!!")
        subknob = item['inst']
        subknob.value = subknob.format.string_to_object (value_str)
        new_value = subknob.format.object_to_string (subknob.value)
        return new_value

    def get_cfg_item_options (self, item):
        tmp_list = []
        if  item['type'].upper() == "ENUM_KNOB":
            if type(item['inst'].format) is not EnumFormat:
                raise Exception ("The item is malformatted!!!")
            tmp_list = [i.name for i in item['inst'].format.values]
        elif item['type'].upper() == "BOOL_KNOB":
            if type(item['inst'].format) is not BoolFormat:
                raise Exception ("The item is malformatted!!!")
            tmp_list = ['true', 'false']
        return  tmp_list

    def get_page_title(self, page_id, top = None):
        if top is None:
            top = self.get_cfg_page()['root']
        for node in top['child']:
            page_key = next(iter(node))
            if page_id == page_key:
                return node[page_key]['title']
            else:
                result = self.get_page_title (page_id, node[page_key])
                if result is not None:
                    return result
        return None

    def get_item_by_path (self, path):
        for each in self.knob_shim:
            path_list = path.split('.')
            namespace = path_list[0]
            var_path = '.'.join(path_list[1:])
            if each['inst'].knob.namespace == namespace and\
              each['inst'].name == var_path and\
              each['inst'].leaf == True:
                return each
        return None

    def add_cfg_page(self, child, parent='Non-core', title=''):
        def _add_cfg_page(cfg_page, child, parent):
            key = next(iter(cfg_page))
            if parent == key:
                cfg_page[key]['child'].append({child: {'title': title,
                                                       'child': []}})
                return True
            else:
                result = False
                for each in cfg_page[key]['child']:
                    if _add_cfg_page(each, child, parent):
                        result = True
                        break
                return result

        return _add_cfg_page(self._cfg_page, child, parent)

    def build_cfg_list (self):

        # Add the pages for each root knob
        for knob in self.schema.knobs:
            if not self.add_cfg_page(child='.'.join([knob.namespace, knob.name]), parent=knob.namespace, title=knob.name):
                # The parent page may not exist yet
                self.add_cfg_page(child=knob.namespace, title=knob.namespace)
                if not self.add_cfg_page(child='.'.join([knob.namespace, knob.name]), parent=knob.namespace, title=knob.name):
                    raise Exception ("Failed to add page for knob %s" % knob.name)

        ret_list = []
        # below is a shim layer that connects the UI and data structure
        for idx, data in enumerate(self.schema.subknobs):
            itype = type(data.format)
            name = data.name
            ord_dict = OrderedDict()
            if itype is IntValueFormat:
                ord_dict['type'] = 'INTEGER_KNOB'
            elif itype is FloatValueFormat:
                ord_dict['type'] = 'FLOAT_KNOB'
            elif itype is BoolFormat:
                ord_dict['type'] = 'BOOL_KNOB'
            elif itype is EnumFormat:
                ord_dict['type'] = 'ENUM_KNOB'
            elif itype is StructFormat:
                ord_dict['type'] = 'STRUCT_KNOB'
            elif itype is ArrayFormat:
                ord_dict['type'] = 'ARRAY_KNOB'
            else:
                raise Exception("Unrecognized data format %s!" % str(itype))
            ord_dict['inst'] = data
            ord_dict['order'] = idx
            ord_dict['name'] = name
            ord_dict['cname'] = name
            ord_dict['value'] = data.format.object_to_string(data.value)
            ord_dict['path'] = '.'.join([data.knob.namespace, name])
            ord_dict['help'] = data.help
            ret_list.append(ord_dict)

        return ret_list

    def sync_shim_and_schema (self):
        for shim in self.knob_shim:
            data = shim['inst']
            shim['value'] = data.format.object_to_string(data.value)

    def load_from_svd (self, path):
        def handler(id, value):
            if id is not None and id.startswith("Device.RuntimeData.RuntimeData"):
                # this is the full svd, it is a base64 encoded bin
                base64_val = value.strip()
                bin_data = base64.b64decode(base64_val)
                self.load_default_from_bin(bin_data, True)

        a = DFCI_SupportLib()

        a.iterate_each_setting(path, handler)

    def load_default_from_bin (self, bin_data, variable_list_format):
        var_list = read_vlist_from_buffer (bin_data)
        # YAML variable is included in list, XML tree should not contain it
        for var in var_list:
          if str(var.guid).lower() == DFCI_SETTINGS_REQUEST_GUID.lower():
            # delete from variable list
            var_list.remove(var)
        uefi_variables_to_knobs (self.schema, var_list)
        self.sync_shim_and_schema ()

    def generate_binary_array (self):
        return vlist_to_binary (self.schema)

    def generate_binary (self, bin_file_name):
        bin_file = open(bin_file_name, "wb")
        bin_file.write (self.generate_binary_array ())
        bin_file.close()
        return 0

    def override_default_value(self, csv_file):
        read_csv (self.schema, csv_file)
        self.sync_shim_and_schema ()
        return 0

    def generate_delta_file_from_bin (self, delta_file, old_data, new_data, full=False):
        self.load_default_from_bin (new_data, True)
        write_csv (self.schema, delta_file, subknobs=full)
        return 0

    def generate_csv_file(self, delta_file, bin_file, bin_file2, full=False):
        fd = open (bin_file, 'rb')
        new_data = bytearray(fd.read())
        fd.close()

        if bin_file2 == '':
            old_data = self.generate_binary_array()
        else:
            old_data = new_data
            fd = open (bin_file2, 'rb')
            new_data = bytearray(fd.read())
            fd.close()

        return self.generate_delta_file_from_bin (delta_file, old_data, new_data, full)


    def load_xml (self, cfg_file):
        self.initialize ()
        # TODO: re-enable the xsd validation once updated
        # dir_path = os.path.dirname(os.path.abspath(__file__))
        # xsd = xmlschema.XMLSchema(os.path.join(dir_path, 'configschema.xsd'))
        # if not xsd.is_valid(cfg_file):
        #     raise Exception ("Input xml does not meet corresponding schema")
        self.schema = Schema.load (cfg_file)

        # Assign all values to their defaults
        for knob in self.schema.knobs:
            knob.value = knob.default

        self.knob_shim = self.build_cfg_list()
        return 0


def usage():
    print ('\n'.join([
          "GenNCCfgData Version 0.1",
          "Usage:",
          "    GenNCCfgData  GENBIN  XmlFile[;CsvFile]   BinOutFile",
          "    GenNCCfgData  GENCSV  XmlFile[;BinFile]   CsvOutFile"
          ]))


def main():
    # Parse the options and args
    argc = len(sys.argv)
    if argc < 4 or argc > 5:
        usage()
        return 1

    gen_cfg_data = CGenNCCfgData()
    command   = sys.argv[1].upper()
    out_file  = sys.argv[3]

    file_list  = sys.argv[2].split(';')
    if len(file_list) >= 2:
        xml_file   = file_list[0]
        csv_file   = file_list[1]
    elif len(file_list) == 1:
        xml_file   = file_list[0]
        csv_file   = ''
    else:
        raise Exception ("ERROR: Invalid parameter '%s' !" % sys.argv[2])

    cfg_bin_file  = ''
    cfg_bin_file2 = ''
    if csv_file:
        if command == "GENCSV":
            cfg_bin_file = csv_file
            csv_file  = ''
            if len(file_list) >= 3:
                cfg_bin_file2 = file_list[2]

    gen_cfg_data.load_xml (xml_file)

    if csv_file:
        gen_cfg_data.override_default_value(csv_file)

    if   command == "GENBIN":
        if len(file_list) == 3:
            old_data = gen_cfg_data.generate_binary_array()
            fi   = open (file_list[2], 'rb')
            new_data = bytearray (fi.read ())
            fi.close ()
            if len(new_data) != len(old_data):
                raise Exception ("Binary file '%s' length does not match, ignored !" % file_list[2])
            else:
                gen_cfg_data.load_default_from_bin (new_data, True)
                gen_cfg_data.override_default_value(csv_file)

        gen_cfg_data.generate_binary(out_file)

    elif command == "GENCSV":
        gen_cfg_data.generate_csv_file (out_file, cfg_bin_file, cfg_bin_file2)

    else:
        raise Exception ("Unsuported command '%s' !" % command)

    return 0


if __name__ == '__main__':
    sys.exit(main())

