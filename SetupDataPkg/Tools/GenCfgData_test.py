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
from GenCfgData import CGenCfgData, array_str_to_value


class UncoreCfgUnitTests(unittest.TestCase):

    # General test for loading yml file and making sure all config is there
    def test_yml_to_config(self):
      cdata = CGenCfgData()
      cdata.load_yaml("samplecfg.yaml", shallow_load=True)
      ui_gen_cfg_data = CGenCfgData()
      ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
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
        if '.' in item['path'] and 'CfgHeader' not in item['path']:
          self.assertEqual(exec[item['cname']]['value'], item['value'])
          self.assertEqual(item['value'], cfg_item['value'])

    # test to load yml, change config, generate a delta file, and load it again
    def test_yml_generate_load_delta(self):
      cdata = CGenCfgData()
      cdata.load_yaml("samplecfg.yaml", shallow_load=True) 
      ui_gen_cfg_data = CGenCfgData()
      ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
      # Merge the UI cfg and data cfg objects
      merged_cfg_tree = cdata.merge_cfg_tree(
          cdata.get_cfg_tree(), ui_gen_cfg_data.get_cfg_tree()
        )
      cdata.set_cfg_tree(merged_cfg_tree)
      cdata.build_cfg_list({'offset': 0})
      cdata.build_var_dict()
      cdata.update_def_value()

      path = 'CGenCfgData_test_dlt.dlt'

      old_data = cdata.generate_binary_array()

      # change one config item, ensure that
      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      cdata.set_config_item_value(item, "New")

      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      # Internally, EditText configs get ' ' added
      self.assertEqual("'New'", item['value'])
      
      # generate delta only file
      cdata.generate_delta_file_from_bin(
                path, old_data, cdata.generate_binary_array(), False
            )

      cdata.set_config_item_value(item, "NewNew")
      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'NewNew'", item['value'])
      
      try:
          cdata.override_default_value(path)
          os.remove(path)
      except Exception as e:
          os.remove(path)
          self.assertTrue(False)

      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'New'", item['value'])

      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('1', item['value'])

      # Test full delta file
      old_data = cdata.generate_binary_array()
      cdata.set_config_item_value(item, '0')
      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('0', item['value'])

      cdata.generate_delta_file_from_bin(
                path, old_data, cdata.generate_binary_array(), True
      )

      cdata.set_config_item_value(item, '1')
      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('1', item['value'])

      try:
          cdata.override_default_value(path)
          os.remove(path)
      except Exception as e:
          os.remove(path)
          self.assertTrue(False)

      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'New'", item['value'])

      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('0', item['value'])

    def test_yml_generate_load_svd(self):
      cdata = CGenCfgData()
      cdata.load_yaml("samplecfg.yaml", shallow_load=True)
      ui_gen_cfg_data = CGenCfgData()
      ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
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
      old_data = cdata.generate_binary_array()

      # change one config item
      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      cdata.set_config_item_value(item, '0')

      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('0', item['value'])

      base64_path = path
      temp_file = path + ".tmp"

      new_data = cdata.generate_binary_array()

      (execs, bytes_array) = cdata.generate_delta_svd_from_bin(
          old_data, new_data
      )

      settings = []
      for index in range(len(execs)):
          b64data = base64.b64encode(bytes_array[index])
          cfg_hdr = cdata.get_item_by_index(
              execs[index]["CfgHeader"]["indx"]
          )
          tag_val = array_str_to_value(cfg_hdr["value"]) >> 20
          settings.append(
              ("Device.ConfigData.TagID_%08x" % tag_val, b64data.decode("utf-8"))
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

      cdata.load_from_svd(path)
      os.remove(path)

      # Check if value is changed
      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('0', item['value'])

      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'PlatName'", item['value'])

      old_data = cdata.generate_binary_array()

      # Check Full SVD
      cdata.set_config_item_value(item, "Diff")
      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'Diff'", item['value'])

      settings = []
      b64data = base64.b64encode(cdata.generate_binary_array())
      settings.append(("Device.ConfigData.ConfigData", b64data.decode("utf-8")))
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

      cdata.set_config_item_value(item, "NewDiff")
      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'NewDiff'", item['value'])

      cdata.load_from_svd(path)
      os.remove(path)

      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'Diff'", item['value'])

      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('0', item['value'])

    def test_yml_generate_load_bin(self):
      cdata = CGenCfgData()
      cdata.load_yaml("samplecfg.yaml", shallow_load=True)
      ui_gen_cfg_data = CGenCfgData()
      ui_gen_cfg_data.load_yaml("samplecfg_UI.yaml", shallow_load=True)
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

      with open(path, "wb") as fd:
            bins = b''
            bins += cdata.generate_binary_array()
            fd.write(bins)

      cdata.set_config_item_value(item, '1')

      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('1', item['value'])

      with open(path, "rb") as fd:
            bin_data = bytearray(fd.read())
            cdata.load_default_from_bin(bin_data, False)
      os.remove(path)

      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('0', item['value'])

      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'PlatName'", item['value'])

      # Test VarList Bin
      cdata.set_config_item_value(item, 'NewDiff')
      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'NewDiff'", item['value'])

      with open(path, "wb") as fd:
            temp_file = path + '.tmp'
            bins = b''
            svd = ''
            b64data = base64.b64encode(cdata.generate_binary_array())
            settings = []
            settings.append(("Device.ConfigData.ConfigData", b64data.decode("utf-8")))
            set_lib = SettingsXMLLib()
            set_lib.create_settings_xml(
                filename=temp_file, version=1, lsv=1, settingslist=settings
            )

            # To remove the line ends and spaces
            with open(temp_file, "r") as tf:
                      for line in tf:
                          svd += line.strip().rstrip("\n")

            os.remove(temp_file)
            bins += cdata.generate_var_list_from_svd(svd)
            fd.write(bins)

      cdata.set_config_item_value(item, 'DiffNew')
      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'DiffNew'", item['value'])

      with open(path, "rb") as fd:
            bin_data = bytearray(fd.read())
            cdata.load_default_from_bin(bin_data, True)
      os.remove(path)

      item = cdata.get_item_by_path('PLATFORM_CFG_DATA.PlatformName')
      self.assertEqual("'NewDiff'", item['value'])

      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('0', item['value'])

if __name__ == '__main__':
    unittest.main()