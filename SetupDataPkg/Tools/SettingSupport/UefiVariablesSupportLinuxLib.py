# @file
#
# Python lib to support Reading and writing UEFI variables from Linux
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent

import os
import uuid
import sys
import struct

from ctypes import (
    create_string_buffer
)

EFI_VAR_MAX_BUFFER_SIZE = 1024 * 1024

class UefiVariable(object):
    ERROR_ENVVAR_NOT_FOUND = 0xcb

    def __init__(self):
        pass
    #
    # Helper function to create buffer for var read/write
    #
    def CreateBuffer(self, init, size=None):
        """CreateBuffer(aString) -> character array
        CreateBuffer(anInteger) -> character array
        CreateBuffer(aString, anInteger) -> character array
        """
        if isinstance(init, str):
            if size is None:
                size = len(init) + 1
            buftype = c_char * size
            buf = buftype()
            buf.value = init
            return buf
        elif isinstance(init, int):
            buftype = c_char * init
            buf = buftype()
            return buf
        raise TypeError(init)

    #
    # Function to get variable
    # return a tuple of error code and variable data as string
    #
    def GetUefiVar(self, name, guid):
        # success
        err = 0
        # the variable name is VariableName-Guid
        path = '/sys/firmware/efi/efivars/' + name + '-%s' % guid

        if not os.path.exists(path):
            err = UefiVariable.ERROR_ENVVAR_NOT_FOUND
            return (err, None)

        efi_var = create_string_buffer(EFI_VAR_MAX_BUFFER_SIZE)
        with open(path, 'rb') as fd:
            efi_var = fd.read()

        return (err, efi_var)

    #
    # Function to get all variable names
    # return a tuple of error code and variable names byte array formatted as:
    #
    # typedef struct _VARIABLE_NAME {
    #   ULONG NextEntryOffset;
    #   GUID VendorGuid;
    #   WCHAR Name[ANYSIZE_ARRAY];
    # } VARIABLE_NAME, *PVARIABLE_NAME;
    #
    def GetUefiAllVarNames(self):
        # success
        status = 0

        # implementation borrowed from https://github.com/awslabs/python-uefivars/blob/main/pyuefivars/efivarfs.py
        path = '/sys/firmware/efi/efivars'
        if not os.path.exists(path):
            status = UefiVariable.ERROR_ENVVAR_NOT_FOUND
            return (status, None)

        vars = os.listdir(path)

        # get the total buffer length, converting to unicode
        length = 0
        offset = 0
        for var in vars:
            split_string = var.split('-')
            name = '-'.join(split_string[:-5])
            name = name.encode('utf-16-le')
            name_len = len(name)
            length += (4 + 16 + name_len)

        efi_var_names = create_string_buffer(length)

        for var in vars:
            # efivarfs stores vars as NAME-GUID
            split_string = var.split('-')
            try:
                # GUID is last 5 elements of split_string
                guid = uuid.UUID('-'.join(split_string[-5:])).bytes_le
            except ValueError:
                raise Exception(f'Could not parse "{var}"')

            # the other part is the name
            name = '-'.join(split_string[:-5])
            name = name.encode('utf-16-le')
            name_len = len(name)

            # NextEntryOffset
            struct.pack_into('<I', efi_var_names, offset, 4 + 16 + name_len)
            offset += 4

            # VendorGuid
            struct.pack_into('=16s', efi_var_names, offset, guid)
            offset += 16

            # Name
            struct.pack_into(f'={name_len}s', efi_var_names, offset, name)
            offset += name_len

        return (status, efi_var_names)

    #
    # Function to set variable
    # return a tuple of boolean status, error_code, error_string (None if not error)
    #
    def SetUefiVar(self, name, guid, var=None, attrs=None):
        var_len = 0
        success = 0  # Fail

        # There is a null terminator at the end of the name
        path = '/sys/firmware/efi/efivars/' + name[:-1] + '-' + str(guid)
        if var is None:
            # we are deleting the variable
            if (os.path.exists(path)):
                os.remove(path)
                success = 1 # expect non-zero success
            return success
        else:
            var_len = len(var)

        if attrs is None:
            attrs = 0x7

        # if the file exists, remove the immutable flag
        if (os.path.exists(path)):
            os.system('sudo chattr -i ' + path)

        with open(path, 'wb') as fd:
            # var data is attribute (UINT32) followed by data
            packed = struct.pack('=I', attrs)
            packed += var
            fd.write(packed)

        return 1
