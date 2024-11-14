
# @file
#
# Module to read infomation such as smbios or MFCI policy
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent

import ctypes
import struct
from edk2toollib.os.uefivariablesupport import UefiVariable


def get_mfci_policy():
    """
    Function to fetch and return the MFCI policy result from UEFI variable.
    Returns the MFCI policy in hexadecimal format or 'Unknown' if an error occurs.
    """
    CURRENT_MFCI_POLICY = "CurrentMfciPolicy"
    MFCI_VENDOR_GUID = "EBA1A9D2-BF4D-4736-B680-B36AFB4DD65B"
    # Load the kernel32.dll library
    UefiVar = UefiVariable()
    (errorcode, data) = UefiVar.GetUefiVar(CURRENT_MFCI_POLICY, MFCI_VENDOR_GUID)

    result = "Unknown"
    if errorcode == 0:
        mfci_value = int.from_bytes(data, byteorder="little", signed=False)
        result = f"0x{mfci_value:016x}"

    return result


def get_schema_xml_hash_from_bios():
    """
    Function to get SCHEMA_XML_HASH variable from BIOS
    Returns the SCHEMA_XML_HASH in hexadecimal format or 'Unknown' if an error occurs.
    """
    SCHEMA_XML_HASH_VAR_NAME = "SCHEMA_XML_HASH"
    SCHEMA_XML_HASH_GUID = "1321e012-14c5-42db-8ca9-e7971d881518"

    UefiVar = UefiVariable()
    (errorcode, data) = UefiVar.GetUefiVar(SCHEMA_XML_HASH_VAR_NAME, SCHEMA_XML_HASH_GUID)

    result = None
    if errorcode == 0:
        # Remove null byte if exist
        if data[-1] == 0:
            data = data[:-1]
        result = data.decode("utf-8")

    return result


def locate_smbios_data():
    # Define constants
    FIRMWARE_TABLE_ID = 0x52534D42  # 'RSMB' ascii signature for smbios table
    SMBIOS_TABLE = 0x53

    # Load the kernel32.dll library
    kernel32 = ctypes.windll.kernel32

    buffer_size = kernel32.GetSystemFirmwareTable(FIRMWARE_TABLE_ID, SMBIOS_TABLE, None, 0)
    buffer = ctypes.create_string_buffer(buffer_size)
    kernel32.GetSystemFirmwareTable(FIRMWARE_TABLE_ID, SMBIOS_TABLE, buffer, buffer_size)

    # Convert the buffer to bytes for easier manipulation
    smbios_data = buffer.raw
    return smbios_data


# Helper function to calculate the total string data of an SMBIOS entry
def calc_smbios_string_len(smbios_data, string_data_offset):

    # Iterate until we find a double-zero sequence indicating the end of the structure
    i = 1
    while smbios_data[string_data_offset + i - 1] != 0 or smbios_data[string_data_offset + i] != 0:
        i += 1

    # Return the total string length
    return i + 1


def locate_smbios_entry(smbios_type):
    found_smbios_entry = []
    smbios_data = locate_smbios_data()

    # Offset the first 8 bytes of SMBIOS entry data
    offset = 8

    # Iterate over all SMBIOS structures until we find given smbios_type
    while offset < len(smbios_data):
        # SMBIOS HEADER Type (1 byte), Length (1 byte), and Handle (2 bytes)
        structure_type, structure_length, handle = struct.unpack_from("<BBH", smbios_data, offset)

        if structure_type == 127:  # End-of-table marker (type 127)
            print("End of SMBIOS table reached.")
            break

        # Calculate the length of the current entry structure
        string_data_offset = offset + structure_length
        string_data_size = calc_smbios_string_len(smbios_data, string_data_offset)
        total_struct_len = string_data_size + structure_length

        if structure_type == smbios_type:
            found_smbios_entry = found_smbios_entry + [smbios_data[offset:offset + total_struct_len]]

        # Move to the next structure
        offset += total_struct_len

    if found_smbios_entry == []:
        print(f"Type {smbios_type} structure not found.")
        return None
    return found_smbios_entry
