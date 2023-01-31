## @ GenNCCfgData.py
#
# Copyright(c) 2020, Intel Corporation. All rights reserved.<BR>
# Copyright(c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import unittest
import copy
import os

from GenNCCfgData import CGenNCCfgData


class UncoreCfgUnitTests(unittest.TestCase):

    # This is a general test for loading xml file and make sure all pages are there
    def test_xml_knob_to_page(self):
        cdata = CGenNCCfgData("sampleschema.xml")
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        self.assertIsNotNone(cdata.schema)
        self.assertIsNotNone(cdata.schema.knobs)

        # {
        #     title: 'title',
        #     child: [{
        #         child1: {
        #             title: 'title',
        #             child: []
        #         }
        #     }]
        # }
        non_core_pages = cdata._cfg_page['root']['child'][0]
        self.assertTrue('sampleschema' in non_core_pages)
        non_core_page = non_core_pages['sampleschema']
        self.assertEqual('sampleschema', non_core_page['title'])

        knob_list = copy.deepcopy(cdata.schema.knobs)
        namespaces = non_core_page['child']
        for namespace in namespaces:
            guid = list(namespace.keys())[0]
            for each in namespace[guid]['child']:
                # the page id here should be namespace.knob
                for page in each:
                    page_ns, name = page.split('.')
                    for knob in knob_list:
                        if knob.name == name and knob.namespace == page_ns:
                            knob_list.remove(knob)

        # make sure each top level knobs has a designated page
        self.assertEqual(len(knob_list), 0)

    # This is a test for loading xml file and make sure all subknobs have a shim instance for rendering
    def test_xml_subknob_to_shim(self):
        cdata = CGenNCCfgData("sampleschema.xml")
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        subknob_list = cdata.schema.subknobs
        for entry in cdata.knob_shim:
            subknob = entry['inst']
            self.assertTrue(subknob in subknob_list)
            subknob_list.remove(subknob)
            # All values should be initialized to default
            self.assertEqual(subknob.value, subknob.default)
            # All values of shim instance should represent that from subknob
            self.assertEqual(entry['value'], subknob.format.object_to_string(subknob.value))
            # The path should prefixed with namespace of the knob group
            self.assertEqual(entry['path'].split('.')[0], subknob.knob.namespace)
            # The help should retain that of the original knob instance
            self.assertEqual(entry['help'], subknob.help)

        # make sure it is one-to-one mapping
        self.assertEqual(len(subknob_list), 0)

    # Retrieving all items with a given page_id, should return all the subknobs under that id
    def test_xml_get_cfg_list(self):
        cdata = CGenNCCfgData("sampleschema.xml")
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        # Get the whole set
        ret = cdata.get_cfg_list()
        self.assertEqual(ret, cdata.knob_shim)

        # Get only a struct knob
        ret = cdata.get_cfg_list('FE3ED49F-B173-41ED-9076-356661D46A42.COMPLEX_KNOB1a')
        self.assertGreater(len(ret), 1)
        self.assertEqual(ret[0]['name'], 'COMPLEX_KNOB1a')
        self.assertEqual(ret[0]['type'], 'STRUCT_KNOB')
        self.assertEqual(cdata.schema.get_knob(
            "FE3ED49F-B173-41ED-9076-356661D46A42", "COMPLEX_KNOB1a"), ret[0]['inst'])
        self.assertEqual(len(ret), len(cdata.schema.get_knob(
            "FE3ED49F-B173-41ED-9076-356661D46A42", "COMPLEX_KNOB1a").knob.subknobs))
        for shim_entry, subknob_entry in zip(ret, cdata.schema.get_knob(
                "FE3ED49F-B173-41ED-9076-356661D46A42", "COMPLEX_KNOB1a").knob.subknobs):
            shim_entry['name'] = subknob_entry.name
            shim_entry['inst'] = subknob_entry

        # Get only a simple knob
        ret = cdata.get_cfg_list('FE3ED49F-B173-41ED-9076-356661D46A42.INTEGER_KNOB')
        self.assertEqual(len(ret), 1)
        self.assertEqual(ret[0]['name'], 'INTEGER_KNOB')
        self.assertEqual(cdata.schema.get_knob("FE3ED49F-B173-41ED-9076-356661D46A42", "INTEGER_KNOB"), ret[0]['inst'])

    # Retrieving values for all shim items, should return the same value from the underlying knob
    def test_xml_get_cfg_item_value(self):
        cdata = CGenNCCfgData("sampleschema.xml")
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        # Get only a struct knob
        for each in cdata.knob_shim:
            val = cdata.get_cfg_item_value(each, array=True)
            self.assertEqual(val, each['inst'].format.object_to_binary(each['inst'].value))

    # Reformat a value string should return properly
    def test_xml_reformat_value_str(self):
        cdata = CGenNCCfgData("sampleschema.xml")
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        # Run on an integer knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.INTEGER_KNOB')
        val_str = '1234'
        ret_str = cdata.reformat_value_str(val_str, cdata.get_cfg_item_length(ret), item=ret)
        self.assertEqual(val_str, ret_str)

        # Run on an enum knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.COMPLEX_KNOB1b.mode')
        val_str = 'THIRD'
        self.assertEqual(ret['type'], 'ENUM_KNOB')
        ret_str = cdata.reformat_value_str(val_str, cdata.get_cfg_item_length(ret), item=ret)
        self.assertEqual(val_str, ret_str)

        # Run on a boolean knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.BOOLEAN_KNOB')
        val_str = 'false'
        self.assertEqual(ret['type'], 'BOOL_KNOB')
        ret_str = cdata.reformat_value_str(val_str, cdata.get_cfg_item_length(ret), item=ret)
        self.assertEqual(val_str, ret_str)

        # Run on a float knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.FLOAT_KNOB')
        val_str = '0.618'
        self.assertEqual(ret['type'], 'FLOAT_KNOB')
        ret_str = cdata.reformat_value_str(val_str, cdata.get_cfg_item_length(ret), item=ret)
        self.assertEqual(val_str, ret_str)

    # Get value should return subknob value, matching the corresponding shim entry
    def test_xml_get_value(self):
        cdata = CGenNCCfgData("sampleschema.xml")
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        # Get only a struct knob
        for each in cdata.knob_shim:
            val = cdata.get_value(each, cdata.get_cfg_item_length(each), array=True)
            self.assertEqual(val, each['inst'].format.object_to_binary(each['inst'].value))

    # Set item value should update the subknob value
    def test_xml_set_item_value(self):
        cdata = CGenNCCfgData("sampleschema.xml")
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        # Run on an integer knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.INTEGER_KNOB')
        val_str = '1234'
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 1234)

        # Run on an enum knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.COMPLEX_KNOB1b.mode')
        val_str = 'THIRD'
        self.assertEqual(ret['type'], 'ENUM_KNOB')
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 2)

        # Run on a boolean knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.BOOLEAN_KNOB')
        val_str = 'false'
        self.assertEqual(ret['type'], 'BOOL_KNOB')
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 0)

        # Run on a float knob
        # floats are by nature imprecise if this is read back again from cdata, it will
        # likely show a different value, due to rounding errors. Doubles are recommended instead.
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.FLOAT_KNOB')
        val_str = '0.618'
        self.assertEqual(ret['type'], 'FLOAT_KNOB')
        ret_str = cdata.set_item_value(val_str, item=ret)
        self.assertEqual(ret_str, val_str)
        self.assertEqual(ret['inst'].value, 0.618)

    # Get item options should reflect the available enum or boolean options
    def test_xml_get_cfg_item_options(self):
        cdata = CGenNCCfgData("sampleschema.xml")
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        # Run on an enum knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.COMPLEX_KNOB1b.mode')
        self.assertEqual(ret['type'], 'ENUM_KNOB')
        ret_list = cdata.get_cfg_item_options(item=ret)
        self.assertEqual(ret_list, ["FIRST", "SECOND", "THIRD"])

        # Run on a boolean knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.BOOLEAN_KNOB')
        self.assertEqual(ret['type'], 'BOOL_KNOB')
        ret_list = cdata.get_cfg_item_options(item=ret)
        self.assertEqual(ret_list, ["true", "false"])

    # Sync function should sync up the value for shim and underlying knobs
    def test_xml_sync_shim_and_schema(self):
        cdata = CGenNCCfgData("sampleschema.xml")
        if os.path.exists("sampleschema.xml"):
            # Load for local testing
            cdata.load_xml("sampleschema.xml")
        elif os.path.exists("SetupDataPkg/Tools/sampleschema.xml"):
            # Load for Linux CI
            cdata.load_xml("SetupDataPkg/Tools/sampleschema.xml")
        else:
            # Load for Windows CI
            cdata.load_xml("SetupDataPkg\\Tools\\sampleschema.xml")

        # Run on an integer knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.INTEGER_KNOB')
        int_str = '1234'
        ret_str = cdata.set_item_value(int_str, item=ret)
        self.assertNotEqual(ret_str, ret['value'])

        # Run on an enum knob
        ret = cdata.get_item_by_path('FE3ED49F-B173-41ED-9076-356661D46A42.COMPLEX_KNOB1b.mode')
        enum_str = 'THIRD'
        ret_str = cdata.set_item_value(enum_str, item=ret)
        self.assertNotEqual(ret_str, ret['value'])

        cdata.sync_shim_and_schema()

        for each in cdata.knob_shim:
            self.assertEqual(each['value'], each['inst'].format.object_to_string(each['inst'].value))


if __name__ == '__main__':
    unittest.main()
