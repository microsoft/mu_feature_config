## @ CommonUtility_test.py
# Common utility unit test script
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

##
# Import Modules
#
import unittest
import CommonUtility


class CommonUtilityTest(unittest.TestCase):

    # Normal print bytes should succeed
    def test_print_bytes(self):
        CommonUtility.print_bytes(b'deadbeef')
        CommonUtility.print_bytes(b'feedf00d')
        CommonUtility.print_bytes(b'')

    # Get bits from bytes should return as expected
    def test_get_bits_from_bytes(self):
        ret = CommonUtility.get_bits_from_bytes(b'deadbeef', 0, 8)
        self.assertEqual(ret, ord('d'))
        ret = CommonUtility.get_bits_from_bytes(b'deadbeef', 8, 1)
        self.assertEqual(ret, (ord('e') & 0x1))
        ret = CommonUtility.get_bits_from_bytes(b'Q', 4, 4)
        self.assertEqual(ret, (ord('Q') & 0xF0) >> 4)

    # Set bits to bytes should return as expected
    def test_set_bits_to_bytes(self):
        input = bytearray(b'deadbeef')
        CommonUtility.set_bits_to_bytes(input, 0, 8, ord('h'))
        self.assertEqual(input, b'headbeef')

        input = bytearray(b'deadbeef')
        CommonUtility.set_bits_to_bytes(input, 24, 8, ord('f'))
        self.assertEqual(input, b'deafbeef')

        input = bytearray(b'deadbeef')
        CommonUtility.set_bits_to_bytes(input, 25, 4, 9)
        self.assertEqual(input, b'dearbeef')

    # Test value to bytes should return as expected
    def test_value_to_bytes(self):
        ret = CommonUtility.value_to_bytes(0xdeadbeef, 4)
        self.assertEqual(ret, bytes([0xef, 0xbe, 0xad, 0xde]))

        input = bytearray(b'deadbeef')
        CommonUtility.set_bits_to_bytes(input, 24, 8, ord('f'))
        self.assertEqual(input, b'deafbeef')

        input = bytearray(b'deadbeef')
        CommonUtility.set_bits_to_bytes(input, 25, 4, 9)
        self.assertEqual(input, b'dearbeef')


if __name__ == '__main__':
    unittest.main()
