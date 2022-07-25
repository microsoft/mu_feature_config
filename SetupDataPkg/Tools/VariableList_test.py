# @ VariableList_test.py
#
# Copyright (c) 2022, Microsoft Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#

import unittest
from xml.dom.minidom import parseString

from VariableList import Schema, ParseError, InvalidNameError


class SchemaParseUnitTests(unittest.TestCase):
    schemaTemplate = """<ConfigSchema>

  <Enums>
    <Enum name="EMPTY_ENUM" help="An empty enum">
    </Enum>
    <Enum name="OPTION_ENUM" help="Modes of the option">
      <Value name="FIRST" value="0" help="First mode" />
      <Value name="SECOND" value="1 " help="Second mode" />
      <Value name="THIRD" value=" 2" help="Third mode" />
    </Enum>
  </Enums>

  <Structs>
    <Struct name="simple_t" help="Simple struct">
      <Member name="value" type="uint32_t" help="Value of a member" />
    </Struct>
    <Struct name="array_t" help="Struct with an array">
      <Member name="value" count="4" type="uint32_t" />
    </Struct>
    <Struct name="multi_t" help="Struct with multiple elements">
      <Member name="value1" type="uint32_t" />
      <Member name="value2" type="uint16_t " />
      <Member name="value3" type=" uint16_t" />
    </Struct>
    <Struct name="packed_t" help="Struct with complex packing">
      <Member name="value1" type="uint8_t" />
      <Member name="value2" type="uint32_t" />
      <Member name="value3" type="uint16_t" />
    </Struct>
    <Struct name="nested_t" help="Struct with nesting">
      <Member name="sub1" type="simple_t" />
      <Member name="value2" type="uint32_t" />
      <Member name="sub3" type="array_t" />
    </Struct>
  </Structs>

  <!-- namespace indicates the GUID namespace the values are stored in -->
  <Knobs namespace="{FE3ED49F-B173-41ED-9076-356661D46A42}">
      <Knob name="k1" type="array_t" default="{{1,2,3,4}}" />
      <Knob name="k1" type="array_t" default="{{1,2,3,4}}" />
  </Knobs>


</ConfigSchema>"""

    # Generates a dom with an xml snippet added to the template
    def insert_xml(self, xml):
        # Parse the template
        dom = parseString(self.schemaTemplate)
        # Parse the snippet to insert
        insert_dom = parseString(xml)
        # Import the first (only) element of the snippet
        n = dom.importNode(insert_dom.childNodes[0], deep=True)
        # Insert it as a child of the root element of the template
        dom.childNodes[0].appendChild(n)
        return dom

    # Test the basic template (which should have no errors)
    def test_golden(self):
        Schema.parse(self.schemaTemplate)
        pass

    def test_missing_enum_value(self):
        dom = self.insert_xml("""
<Enums>
    <Enum name="MISSING_VALUE" help="Enum with missing values">
      <Value name="ELEMENT1" value="0" help="First mode" />
      <Value name="ELEMENT2"           help="Second mode" />
      <Value name="ELEMENT3" value="2" help="Third mode" />
    </Enum>
</Enums>""")
        self.assertRaises(ParseError, Schema, dom)

    def test_invalid_enum_value(self):
        dom = self.insert_xml("""
<Enums>
    <Enum name="INVALID_VALUE" help="Enum with invalid values">
      <Value name="ELEMENT1" value="x" help="First mode" />
    </Enum>
</Enums>""")
        self.assertRaises(ParseError, Schema, dom)

    def test_invalid_enum_value_name(self):
        dom = self.insert_xml("""
<Enums>
    <Enum name="INVALID_VALUE_NAME" help="Enum with invalid values">
      <Value name="1ELEMENT1" value="0" help="First mode" />
    </Enum>
</Enums>""")
        self.assertRaises(InvalidNameError, Schema, dom)

    def test_invalid_enum_name(self):
        dom = self.insert_xml("""
<Enums>
    <Enum name="1INVALID_NAME" help="Enum with invalid values">
      <Value name="ELEMENT1" value="0" help="First mode" />
    </Enum>
</Enums>""")
        self.assertRaises(InvalidNameError, Schema, dom)

    def test_default(self):
        schema = Schema.parse(self.schemaTemplate)

        knob = schema.get_knob("k1")

        # The initial value will be None
        self.assertEqual(knob.value, None)
        # The default value should be valid
        self.assertEqual(knob.default['value'], [1, 2, 3, 4])

        # Get the a subknob and modify a single element
        subknob = schema.get_knob("k1.value[1]")
        subknob.value = 10

        # The value will now be initialized from default,
        # with the single value modified
        self.assertEqual(knob.value['value'], [1, 10, 3, 4])
        # Verify the default hasn't changed
        self.assertEqual(knob.default['value'], [1, 2, 3, 4])

if __name__ == '__main__':
    unittest.main()