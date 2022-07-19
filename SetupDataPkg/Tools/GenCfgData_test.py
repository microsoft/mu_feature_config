## @ GenCfgData_test.py
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import unittest
import os

from GenCfgData import CGenCfgData


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
      cdata.build_cfg_list()
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
      cdata.build_cfg_list()
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
'''

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
      cdata.build_cfg_list()
      cdata.build_var_dict()
      cdata.update_def_value()
      path = 'CGenCfgData_test_svd.svd'

      old_data = cdata.generate_binary_array()

      # change one config item
      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      cdata.set_config_item_value(item, '0')

      item = cdata.get_item_by_path('GFX_CFG_DATA.PowerOnPort0')
      self.assertEqual('0', item['value'])

      cdata.

'''      

if __name__ == '__main__':
    unittest.main()