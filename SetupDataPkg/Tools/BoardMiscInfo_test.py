## @ BoardMiscInfo_test.py
# BoardMiscInfo unit test script
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

##
# Import Modules
#
import unittest
import platform
import BoardMiscInfo


class BoardMiscInfoTest(unittest.TestCase):

    def test_locate_smbios_data_does_not_crash(self):
        """Test that locate_smbios_data handles errors gracefully."""
        # This test verifies the function doesn't crash, even if it can't read SMBIOS
        # On systems without proper permissions or SMBIOS support, it should raise an exception
        system_platform = platform.system()

        if system_platform in ["Windows", "Linux"]:
            try:
                result = BoardMiscInfo.locate_smbios_data()
                # If we get here, SMBIOS data was successfully retrieved
                self.assertIsInstance(result, bytes)
                self.assertGreater(len(result), 0)
            except Exception as e:
                # Expected on systems without permissions or SMBIOS support
                # Just verify the error message is informative
                self.assertIn("SMBIOS", str(e))

    def test_locate_smbios_entry_handles_missing_data(self):
        """Test that locate_smbios_entry returns None when SMBIOS is unavailable."""
        # This should not crash even if SMBIOS data is unavailable
        result = BoardMiscInfo.locate_smbios_entry(0)
        # Result should be either None (no access) or a list (has access)
        self.assertTrue(result is None or isinstance(result, list))

    def test_get_mfci_policy_returns_value(self):
        """Test that get_mfci_policy returns a string."""
        result = BoardMiscInfo.get_mfci_policy()
        self.assertIsInstance(result, str)
        # Should return "Unknown" if the UEFI variable is not available
        self.assertTrue(len(result) > 0)

    def test_get_schema_xml_hash_from_bios_returns_value(self):
        """Test that get_schema_xml_hash_from_bios returns None or string."""
        result = BoardMiscInfo.get_schema_xml_hash_from_bios()
        # Should return None if variable is not available, or a string if it is
        self.assertTrue(result is None or isinstance(result, str))

    def test_calc_smbios_string_len(self):
        """Test the calc_smbios_string_len helper function."""
        # Create test SMBIOS string data with double-zero terminator
        test_data = bytearray(b"String1\x00String2\x00\x00Extra")
        result = BoardMiscInfo.calc_smbios_string_len(test_data, 0)
        # Should find the double-zero at positions 15-16
        self.assertEqual(result, 17)  # "String1\x00String2\x00\x00" = 17 bytes

        # Test with immediate double-zero
        test_data2 = bytearray(b"\x00\x00Extra")
        result2 = BoardMiscInfo.calc_smbios_string_len(test_data2, 0)
        self.assertEqual(result2, 2)  # Just the two zeros


if __name__ == '__main__':
    unittest.main()
