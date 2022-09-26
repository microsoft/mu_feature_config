## @ GenCfgData_test.py
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import unittest
import os
import base64

from SettingSupport.SettingsXMLLib import SettingsXMLLib        # noqa: E402
from GenCfgData import CGenCfgData, DICT_KEYS_KEYWORDS
from GenNCCfgData import CGenNCCfgData
from collections import OrderedDict


class UefiCfgUnitTests(unittest.TestCase):

    # General test for loading yml file and making sure all config is there
    def test_yml_to_config(self):
        def _search_tree(node, item):
            for key in node.keys():
                # ignore the $STRUCT, CFG_HDR, and CondValue metadata
                if key in DICT_KEYS_KEYWORDS:
                    continue
                # only recurse if this is a struct
                if type(node) is OrderedDict and "indx" not in node[key]:
                    result = _search_tree(node[key], item)
                    if result is True:
                        return result
                if item['cname'] == key:
                    if node[key]['value'] == item['value']:
                        return True
            return False

        cdata = CGenCfgData()
        if os.path.exists("samplecfg.yaml"):
            # Load for local testing
            cdata.load_yaml("samplecfg.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg.yaml"):
            # Load for Linux CI
            cdata.load_yaml("SetupDataPkg/Tools/samplecfg.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            cdata.load_yaml("SetupDataPkg\\Tools\\samplecfg.yaml", shallow_load=True)
        ui_gen_cfg_data = CGenCfgData()
        if os.path.exists("samplecfg_UI.yaml"):
            # Load for local testing
            ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg_UI.yaml"):
            # Load for Linux CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg/Tools/samplecfg_UI.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg\\Tools\\samplecfg_UI.yaml", shallow_load=True)
        # Merge the UI cfg and data cfg objects
        merged_cfg_tree = cdata.merge_cfg_tree(
            cdata.get_cfg_tree(), ui_gen_cfg_data.get_cfg_tree()
        )
        cdata.set_cfg_tree(merged_cfg_tree)
        cdata.build_cfg_list({'offset': 0})
        cdata.build_var_dict()
        cdata.update_def_value()

        self.assertIsNotNone(cdata._cfg_list)

        for item in cdata._cfg_list:
            cfg_item = cdata.locate_cfg_item(item['path'])
            exec = cdata.locate_exec_from_item(cfg_item)
            # we only want to check the values that are in YML, not metadata
            if item['type'] != 'Reserved':
                # for multiple embedded structs of the same kind, multiple instances of the same cname will show up in
                # the exec. We will need to find ours.
                if not item['cname'] in exec:
                    found_in_exec = _search_tree(exec, item)
                else:
                    found_in_exec = True
                    self.assertEqual(exec[item['cname']]['value'], item['value'])

                self.assertEqual(found_in_exec, True)
                self.assertEqual(item['value'], cfg_item['value'])

    # test to load yml, change config, generate a delta file, and load it again
    def test_yml_generate_load_delta(self):
        cdata = CGenCfgData()
        if os.path.exists("samplecfg.yaml"):
            # Load for local testing
            cdata.load_yaml("samplecfg.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg.yaml"):
            # Load for Linux CI
            cdata.load_yaml("SetupDataPkg/Tools/samplecfg.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            cdata.load_yaml("SetupDataPkg\\Tools\\samplecfg.yaml", shallow_load=True)
        ui_gen_cfg_data = CGenCfgData()
        if os.path.exists("samplecfg_UI.yaml"):
            # Load for local testing
            ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg_UI.yaml"):
            # Load for Linux CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg/Tools/samplecfg_UI.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg\\Tools\\samplecfg_UI.yaml", shallow_load=True)
        # Merge the UI cfg and data cfg objects
        merged_cfg_tree = cdata.merge_cfg_tree(
            cdata.get_cfg_tree(), ui_gen_cfg_data.get_cfg_tree()
        )
        cdata.set_cfg_tree(merged_cfg_tree)
        cdata.build_cfg_list({'offset': 0})
        cdata.build_var_dict()
        cdata.update_def_value()

        path = 'CGenCfgData_test_dlt.dlt'

        old_data = cdata.generate_binary_array(False)

        # change one config item, ensure that
        item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        cdata.set_config_item_value(item, "New")

        item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        # Internally, EditText configs get ' ' added
        self.assertEqual("'New'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        cdata.set_config_item_value(item, '0xDEAD')

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        cdata.set_config_item_value(item, 'OUT')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        # generate delta only file
        cdata.generate_delta_file_from_bin(
            path, old_data, cdata.generate_binary_array(False), False
        )

        item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        cdata.set_config_item_value(item, "NewNew")
        item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        self.assertEqual("'NewNew'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        cdata.set_config_item_value(item, '0x5')

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0x0005', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        cdata.set_config_item_value(item, '1')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('1', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        cdata.set_config_item_value(item, 'BAD')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'BAD'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        cdata.set_config_item_value(item, '1')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('1', item['value'])

        try:
            cdata.override_default_value(path)
            os.remove(path)
        except:
            os.remove(path)
            self.assertTrue(False)

        item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        self.assertEqual("'New'", item['value'])

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('1', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'IN'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.PortQoSMapping')
        self.assertEqual("'A'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('2', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'2'", item['value'])

        # Test full delta file
        old_data = cdata.generate_binary_array(False)
        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        cdata.set_config_item_value(item, '0')
        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        cdata.set_config_item_value(item, 'WOW')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'WOW'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        cdata.set_config_item_value(item, 'P')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'P'", item['value'])

        cdata.generate_delta_file_from_bin(
            path, old_data, cdata.generate_binary_array(False), True
        )

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        cdata.set_config_item_value(item, '1')
        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('1', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        cdata.set_config_item_value(item, '2')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('2', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        cdata.set_config_item_value(item, 'UP')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'UP'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        cdata.set_config_item_value(item, 'Q')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'Q'", item['value'])

        try:
            cdata.override_default_value(path)
            os.remove(path)
        except:
            os.remove(path)
            self.assertTrue(False)

        item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        self.assertEqual("'New'", item['value'])

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'WOW'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.PortQoSMapping')
        self.assertEqual("'A'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'P'", item['value'])

        # Test to create and then load delta SVD for YML only
    def test_yml_generate_load_svd(self):
        cdata = CGenCfgData()
        if os.path.exists("samplecfg.yaml"):
            # Load for local testing
            cdata.load_yaml("samplecfg.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg.yaml"):
            # Load for Linux CI
            cdata.load_yaml("SetupDataPkg/Tools/samplecfg.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            cdata.load_yaml("SetupDataPkg\\Tools\\samplecfg.yaml", shallow_load=True)
        ui_gen_cfg_data = CGenCfgData()
        if os.path.exists("samplecfg_UI.yaml"):
            # Load for local testing
            ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg_UI.yaml"):
            # Load for Linux CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg/Tools/samplecfg_UI.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg\\Tools\\samplecfg_UI.yaml", shallow_load=True)
        # Merge the UI cfg and data cfg objects
        merged_cfg_tree = cdata.merge_cfg_tree(
            cdata.get_cfg_tree(), ui_gen_cfg_data.get_cfg_tree()
        )
        cdata.set_cfg_tree(merged_cfg_tree)
        cdata.build_cfg_list({'offset': 0})
        cdata.build_var_dict()
        cdata.update_def_value()
        path = 'CGenCfgData_test_svd.svd'

        # Generate delta SVD
        old_data = cdata.generate_binary_array(False)

        # change one config item
        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        cdata.set_config_item_value(item, '0xDEAD')

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        cdata.set_config_item_value(item, 'OUT')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        base64_path = path
        temp_file = path + ".tmp"

        new_data = cdata.generate_binary_array(False)

        (execs, bytes_array) = cdata.generate_delta_svd_from_bin(
            old_data, new_data
        )

        settings = []
        for index in range(len(execs)):
            b64data = base64.b64encode(bytes_array[index])
            settings.append(
                (execs[index], b64data.decode("utf-8"))
            )
        set_lib = SettingsXMLLib()
        set_lib.create_settings_xml(
            filename=temp_file, version=1, lsv=1, settingslist=settings
        )

        # To remove the line ends and spaces
        with open(temp_file, "r") as tf:
            with open(base64_path, "w") as ff:
                for line in tf:
                    line = line.strip().rstrip("\n")
                    ff.write(line)
        os.remove(temp_file)

        # Change value before reloading SVD
        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        cdata.set_config_item_value(item, '1')

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('1', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        cdata.set_config_item_value(item, '0xA5')

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0x00A5', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        cdata.set_config_item_value(item, '1')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('1', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        cdata.set_config_item_value(item, 'BAD')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'BAD'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        cdata.set_config_item_value(item, '1')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('1', item['value'])

        cdata.load_from_svd(path)
        os.remove(path)

        # Check if value is changed
        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        self.assertEqual("'PLAT'", item['value'].rstrip('\0'))

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'IN'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.PortQoSMapping')
        self.assertEqual("'A'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('2', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'2'", item['value'])

        # Test Full SVD
        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        cdata.set_config_item_value(item, 'WOW')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'WOW'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        cdata.set_config_item_value(item, 'P')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'P'", item['value'])
        settings = []
        index = 0
        uefi_var, name = cdata.get_var_by_index(index)
        while uefi_var is not None:
            b64data = base64.b64encode(uefi_var)
            settings.append(
                (name, b64data.decode("utf-8"))
            )
            index += 1
            uefi_var, name = cdata.get_var_by_index(index)

        set_lib = SettingsXMLLib()
        set_lib.create_settings_xml(
            filename=temp_file, version=1, lsv=1, settingslist=settings
        )

        # To remove the line ends and spaces
        with open(temp_file, "r") as tf:
            with open(path, "w") as ff:
                for line in tf:
                    line = line.strip().rstrip("\n")
                    ff.write(line)
        os.remove(temp_file)

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        cdata.set_config_item_value(item, '1')

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('1', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        cdata.set_config_item_value(item, '2')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('2', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        cdata.set_config_item_value(item, 'UP')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'UP'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        cdata.set_config_item_value(item, 'Q')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'Q'", item['value'])

        cdata.load_from_svd(path)
        os.remove(path)

        # Check if value is changed
        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        self.assertEqual("'PLAT'", item['value'].rstrip('\0'))

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'WOW'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.PortQoSMapping')
        self.assertEqual("'A'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'P'", item['value'])

    # Test to create/load varlist bins for YML only
    def test_yml_generate_load_bin(self):
        cdata = CGenCfgData()
        if os.path.exists("samplecfg.yaml"):
            # Load for local testing
            cdata.load_yaml("samplecfg.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg.yaml"):
            # Load for Linux CI
            cdata.load_yaml("SetupDataPkg/Tools/samplecfg.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            cdata.load_yaml("SetupDataPkg\\Tools\\samplecfg.yaml", shallow_load=True)
        ui_gen_cfg_data = CGenCfgData()
        if os.path.exists("samplecfg_UI.yaml"):
            # Load for local testing
            ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg_UI.yaml"):
            # Load for Linux CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg/Tools/samplecfg_UI.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg\\Tools\\samplecfg_UI.yaml", shallow_load=True)
        # Merge the UI cfg and data cfg objects
        merged_cfg_tree = cdata.merge_cfg_tree(
            cdata.get_cfg_tree(), ui_gen_cfg_data.get_cfg_tree()
        )
        cdata.set_cfg_tree(merged_cfg_tree)
        cdata.build_cfg_list({'offset': 0})
        cdata.build_var_dict()
        cdata.update_def_value()
        path = 'CGenCfgData_test_bin.bin'

        # change one config item
        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        cdata.set_config_item_value(item, '0xDEAD')

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        cdata.set_config_item_value(item, 'OUT')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        cdata.set_config_item_value(item, '0')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        with open(path, "wb") as fd:
            bins = b''
            bins += cdata.generate_binary_array(True)
            fd.write(bins)

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        cdata.set_config_item_value(item, '1')

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('1', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        cdata.set_config_item_value(item, '0x5AFF')

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0x5AFF', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        cdata.set_config_item_value(item, '1')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('1', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        cdata.set_config_item_value(item, 'BAD')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'BAD'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        cdata.set_config_item_value(item, '1')

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('1', item['value'])

        with open(path, "rb") as fd:
            bin_data = bytearray(fd.read())
            cdata.load_default_from_bin(bin_data, True)
        os.remove(path)

        item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        self.assertEqual("'PLAT'", item['value'].rstrip('\0'))

        item = cdata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'IN'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.PortQoSMapping')
        self.assertEqual("'A'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('2', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = cdata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'2'", item['value'])

    # General test to load both yml and xml and confirm the config
    def test_merged_yml_xml_generate_load_svd(self):
        # Create yml obj
        ydata = CGenCfgData()
        if os.path.exists("samplecfg.yaml"):
            # Load for local testing
            ydata.load_yaml("samplecfg.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg.yaml"):
            # Load for Linux CI
            ydata.load_yaml("SetupDataPkg/Tools/samplecfg.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            ydata.load_yaml("SetupDataPkg\\Tools\\samplecfg.yaml", shallow_load=True)
        ui_gen_cfg_data = CGenCfgData()
        if os.path.exists("samplecfg_UI.yaml"):
            # Load for local testing
            ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg_UI.yaml"):
            # Load for Linux CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg/Tools/samplecfg_UI.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg\\Tools\\samplecfg_UI.yaml", shallow_load=True)
        # Merge the UI cfg and data cfg objects
        merged_cfg_tree = ydata.merge_cfg_tree(
            ydata.get_cfg_tree(), ui_gen_cfg_data.get_cfg_tree()
        )
        ydata.set_cfg_tree(merged_cfg_tree)
        ydata.build_cfg_list({'offset': 0})
        ydata.build_var_dict()
        ydata.update_def_value()

        # Create xml obj
        cdata = CGenNCCfgData()
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        # Create Delta SVD
        y_old_data = ydata.generate_binary_array(False)
        x_old_data = cdata.generate_binary_array(True)
        item = ydata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        ydata.set_config_item_value(item, "New")

        item = ydata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        # Internally, EditText configs get ' ' added
        self.assertEqual("'New'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        ydata.set_config_item_value(item, '0xDEAD')

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        ydata.set_config_item_value(item, '0')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        ydata.set_config_item_value(item, 'OUT')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        ydata.set_config_item_value(item, '0')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.INTEGER_KNOB')
        val_str = '1234'
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 1234)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.COMPLEX_KNOB1b.mode')
        val_str = 'THIRD'
        self.assertEqual(ret['type'], 'ENUM_KNOB')
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 2)

        settings = []
        y_new_data = ydata.generate_binary_array(False)
        x_new_data = cdata.generate_binary_array(True)
        (name_array, var_array) = ydata.generate_delta_svd_from_bin(y_old_data, y_new_data)
        (xml_name_array, xml_var_array) = cdata.generate_delta_svd_from_bin(x_old_data, x_new_data)
        name_array += xml_name_array
        var_array += xml_var_array

        for index in range(len(name_array)):
            b64data = base64.b64encode(var_array[index])
            settings.append(
                (name_array[index], b64data.decode("utf-8"))
            )

        path = 'merged_xml_yml_svd.svd'
        temp_file = path + '.tmp'
        set_lib = SettingsXMLLib()
        set_lib.create_settings_xml(
            filename=temp_file, version=1, lsv=1, settingslist=settings
        )

        # To remove the line ends and spaces
        with open(temp_file, "r") as tf:
            with open(path, "w") as ff:
                for line in tf:
                    line = line.strip().rstrip("\n")
                    ff.write(line)
        os.remove(temp_file)

        item = ydata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        ydata.set_config_item_value(item, "NewNew")

        item = ydata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        # Internally, EditText configs get ' ' added
        self.assertEqual("'NewNew'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        ydata.set_config_item_value(item, '0x5A')

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0x005A', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        ydata.set_config_item_value(item, '1')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('1', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        ydata.set_config_item_value(item, 'BAD')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'BAD'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        ydata.set_config_item_value(item, '1')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('1', item['value'])

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.INTEGER_KNOB')
        val_str = '4321'
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 4321)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.COMPLEX_KNOB1b.mode')
        val_str = 'SECOND'
        self.assertEqual(ret['type'], 'ENUM_KNOB')
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 1)

        ydata.load_from_svd(path)
        cdata.load_from_svd(path)
        os.remove(path)

        item = ydata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        # Internally, EditText configs get ' ' added
        self.assertEqual("'New'", item['value'])

        item = ydata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('1', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'IN'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.PortQoSMapping')
        self.assertEqual("'A'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('2', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'2'", item['value'])

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.INTEGER_KNOB')
        self.assertEqual(ret['inst'].value, 1234)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.COMPLEX_KNOB1b.mode')
        self.assertEqual(ret['inst'].value, 2)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.BOOLEAN_KNOB')
        self.assertEqual(True, ret['inst'].value)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.DOUBLE_KNOB')
        self.assertEqual(3.1415926, ret['inst'].value)

        # Test Full SVD
        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        ydata.set_config_item_value(item, '0')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        ydata.set_config_item_value(item, 'WOW')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'WOW'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        ydata.set_config_item_value(item, 'P')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'P'", item['value'])
        settings = []
        index = 0
        uefi_var, name = cdata.get_var_by_index(index)
        while uefi_var is not None:
            b64data = base64.b64encode(uefi_var)
            settings.append(
                (name, b64data.decode("utf-8"))
            )
            index += 1
            uefi_var, name = cdata.get_var_by_index(index)

        index = 0
        uefi_var, name = ydata.get_var_by_index(index)
        while uefi_var is not None:
            b64data = base64.b64encode(uefi_var)
            settings.append(
                (name, b64data.decode("utf-8"))
            )
            index += 1
            uefi_var, name = ydata.get_var_by_index(index)

        set_lib = SettingsXMLLib()
        set_lib.create_settings_xml(
            filename=temp_file, version=1, lsv=1, settingslist=settings
        )

        # To remove the line ends and spaces
        with open(temp_file, "r") as tf:
            with open(path, "w") as ff:
                for line in tf:
                    line = line.strip().rstrip("\n")
                    ff.write(line)
        os.remove(temp_file)

        item = ydata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        ydata.set_config_item_value(item, "AltF4")

        item = ydata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        # Internally, EditText configs get ' ' added
        self.assertEqual("'AltF4'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        ydata.set_config_item_value(item, '2')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('2', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        ydata.set_config_item_value(item, 'UP')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'UP'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        ydata.set_config_item_value(item, 'Q')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'Q'", item['value'])

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.INTEGER_KNOB')
        val_str = '5995'
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 5995)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.COMPLEX_KNOB1b.mode')
        val_str = 'THIRD'
        self.assertEqual(ret['type'], 'ENUM_KNOB')
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 2)

        ydata.load_from_svd(path)
        cdata.load_from_svd(path)
        os.remove(path)

        item = ydata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        # Internally, EditText configs get ' ' added
        self.assertEqual("'New'", item['value'])

        item = ydata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('1', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'WOW'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.PortQoSMapping')
        self.assertEqual("'A'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'P'", item['value'])

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.INTEGER_KNOB')
        self.assertEqual(ret['inst'].value, 1234)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.COMPLEX_KNOB1b.mode')
        self.assertEqual(ret['inst'].value, 2)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.BOOLEAN_KNOB')
        self.assertEqual(True, ret['inst'].value)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.DOUBLE_KNOB')
        self.assertEqual(3.1415926, ret['inst'].value)

    def test_merged_xml_yml_generate_load_bin(self):
        # Create yml obj
        ydata = CGenCfgData()
        if os.path.exists("samplecfg.yaml"):
            # Load for local testing
            ydata.load_yaml("samplecfg.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg.yaml"):
            # Load for Linux CI
            ydata.load_yaml("SetupDataPkg/Tools/samplecfg.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            ydata.load_yaml("SetupDataPkg\\Tools\\samplecfg.yaml", shallow_load=True)
        ui_gen_cfg_data = CGenCfgData()
        if os.path.exists("samplecfg_UI.yaml"):
            # Load for local testing
            ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
        elif os.path.exists("SetupDataPkg/Tools/samplecfg_UI.yaml"):
            # Load for Linux CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg/Tools/samplecfg_UI.yaml", shallow_load=True)
        else:
            # Load for Windows CI
            ui_gen_cfg_data.load_yaml("SetupDataPkg\\Tools\\samplecfg_UI.yaml", shallow_load=True)
        # Merge the UI cfg and data cfg objects
        merged_cfg_tree = ydata.merge_cfg_tree(
            ydata.get_cfg_tree(), ui_gen_cfg_data.get_cfg_tree()
        )
        ydata.set_cfg_tree(merged_cfg_tree)
        ydata.build_cfg_list({'offset': 0})
        ydata.build_var_dict()
        ydata.update_def_value()

        # Create xml obj
        cdata = CGenNCCfgData()
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        # Create VarList bin
        item = ydata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        ydata.set_config_item_value(item, "0")

        item = ydata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual("0", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        ydata.set_config_item_value(item, '0xDEAD')

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        ydata.set_config_item_value(item, '0')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        ydata.set_config_item_value(item, 'OUT')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        ydata.set_config_item_value(item, '0')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.BOOLEAN_KNOB')
        val_str = 'false'
        cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret['inst'].value, 0)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.FLOAT_KNOB')
        val_str = '0.618'
        cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret['inst'].value, 0.618)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.DOUBLE_KNOB')
        val_str = '5.293845'
        cdata.set_item_value(val_str, item=ret)
        self.assertEqual(5.293845, ret['inst'].value)

        path = 'merged_xml_yml_bin.bin'

        with open(path, "wb") as fd:
            bins = b''
            bins += ydata.generate_binary_array(True)
            bins += cdata.generate_binary_array(True)
            fd.write(bins)

        item = ydata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        ydata.set_config_item_value(item, "1")

        item = ydata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual("1", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        ydata.set_config_item_value(item, '0x5A')

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0x005A', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        ydata.set_config_item_value(item, '1')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('1', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        ydata.set_config_item_value(item, 'BAD')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'BAD'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        ydata.set_config_item_value(item, '1')

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('1', item['value'])

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.BOOLEAN_KNOB')
        val_str = 'true'
        cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret['inst'].value, True)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.DOUBLE_KNOB')
        val_str = '5.293845'
        cdata.set_item_value(val_str, item=ret)
        self.assertEqual(5.293845, ret['inst'].value)

        with open(path, "rb") as fd:
            bin_data = bytearray(fd.read())

        ydata.load_default_from_bin(bin_data, True)
        cdata.load_default_from_bin(bin_data, True)
        os.remove(path)

        item = ydata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
        self.assertEqual("'PLAT'", item['value'].rstrip('\0'))

        item = ydata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Flags')
        self.assertEqual('0xDEAD', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.Overdrive')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortDescription')
        self.assertEqual("'IN'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port0.PortEnable.PortQoSMapping')
        self.assertEqual("'A'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.Overdrive')
        self.assertEqual('2', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortDescription')
        self.assertEqual("'OUT'", item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.EnablePort')
        self.assertEqual('0', item['value'])

        item = ydata.get_item_by_path('IO_CFG_DATA.Port1.PortEnable.PortQoSMapping')
        self.assertEqual("'2'", item['value'])

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.INTEGER_KNOB')
        self.assertEqual(ret['inst'].value, 100)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.COMPLEX_KNOB1a.mode')
        self.assertEqual(ret['inst'].value, 0)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.BOOLEAN_KNOB')
        self.assertEqual(False, ret['inst'].value)

        ret = cdata.get_item_by_path('{FE3ED49F-B173-41ED-9076-356661D46A42}.DOUBLE_KNOB')
        self.assertEqual(5.293845, ret['inst'].value)


if __name__ == '__main__':
    unittest.main()
