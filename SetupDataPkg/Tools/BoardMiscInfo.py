
# @file
#
# Module to read infomation such as smbios or MFCI policy
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent

import ctypes
import struct
import platform
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
    """
    Locate and return SMBIOS data.

    This function works on both Windows and Linux platforms:
    - Windows: Uses GetSystemFirmwareTable API via kernel32.dll
    - Linux: Reads from /sys/firmware/dmi/tables/DMI

    Returns:
        bytes: Raw SMBIOS data, or None if unavailable

    Raises:
        Exception: If SMBIOS data cannot be retrieved
    """
    system_platform = platform.system()

    if system_platform == "Windows":
        try:
            # Define constants
            FIRMWARE_TABLE_ID = 0x52534D42  # 'RSMB' ascii signature for smbios table
            SMBIOS_TABLE = 0x53

            # Load the kernel32.dll library
            kernel32 = ctypes.windll.kernel32

            buffer_size = kernel32.GetSystemFirmwareTable(FIRMWARE_TABLE_ID, SMBIOS_TABLE, None, 0)
            if buffer_size == 0:
                raise Exception("Failed to get SMBIOS table size on Windows")

            buffer = ctypes.create_string_buffer(buffer_size)
            kernel32.GetSystemFirmwareTable(FIRMWARE_TABLE_ID, SMBIOS_TABLE, buffer, buffer_size)

            # Convert the buffer to bytes for easier manipulation
            smbios_data = buffer.raw
            return smbios_data
        except Exception as e:
            raise Exception(f"Failed to retrieve SMBIOS data on Windows: {e}")

    elif system_platform == "Linux":
        try:
            # On Linux, SMBIOS data is available in /sys/firmware/dmi/tables/DMI
            dmi_path = "/sys/firmware/dmi/tables/DMI"
            with open(dmi_path, "rb") as dmi_file:
                smbios_data = dmi_file.read()
            return smbios_data
        except PermissionError:
            raise Exception("Permission denied reading SMBIOS data. Root/sudo access may be required on Linux.")
        except FileNotFoundError:
            raise Exception("SMBIOS data not available at /sys/firmware/dmi/tables/DMI")
        except Exception as e:
            raise Exception(f"Failed to retrieve SMBIOS data on Linux: {e}")

    else:
        raise Exception(f"SMBIOS data retrieval not supported on platform: {system_platform}")


# Helper function to calculate the total string data of an SMBIOS entry
def calc_smbios_string_len(smbios_data, string_data_offset):

    # Iterate until we find a double-zero sequence indicating the end of the structure
    i = 1
    while smbios_data[string_data_offset + i - 1] != 0 or smbios_data[string_data_offset + i] != 0:
        i += 1

    # Return the total string length
    return i + 1


def locate_smbios_entry(smbios_type):
    """
    Locate SMBIOS entries of a specific type.

    Args:
        smbios_type: The SMBIOS structure type to search for

    Returns:
        list: List of SMBIOS entries of the specified type, or None if not found or on error
    """
    found_smbios_entry = []

    try:
        smbios_data = locate_smbios_data()
    except Exception as e:
        print(f"Warning: Could not retrieve SMBIOS data: {e}")
        return None

    # Offset the first 8 bytes of SMBIOS entry data on Windows
    # Linux DMI file doesn't have this header, so detect based on platform
    system_platform = platform.system()
    offset = 8 if system_platform == "Windows" else 0

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
