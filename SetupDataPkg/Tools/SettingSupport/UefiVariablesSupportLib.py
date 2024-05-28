# @file
#
# Python lib to support Reading and writing UEFI variables from windows
#
# Copyright (c), Microsoft Corporation
# SPDX-License-Identifier: BSD-2-Clause-Patent

from ctypes import (
    windll,
    c_wchar_p,
    c_void_p,
    c_int,
    create_string_buffer,
    WinError,
    pointer
)
from ctypes.wintypes import DWORD
import logging
from win32 import win32api
from win32 import win32process
from win32 import win32security

kernel32 = windll.kernel32
EFI_VAR_MAX_BUFFER_SIZE = 1024 * 1024


class UefiVariable(object):
    ERROR_ENVVAR_NOT_FOUND = 0xcb

    def __init__(self):
        # enable required SeSystemEnvironmentPrivilege privilege
        privilege = win32security.LookupPrivilegeValue(
            None, "SeSystemEnvironmentPrivilege"
        )
        token = win32security.OpenProcessToken(
            win32process.GetCurrentProcess(),
            win32security.TOKEN_READ | win32security.TOKEN_ADJUST_PRIVILEGES,
        )
        win32security.AdjustTokenPrivileges(
            token, False, [(privilege, win32security.SE_PRIVILEGE_ENABLED)]
        )
        win32api.CloseHandle(token)

        # import firmware variable API
        try:
            self._GetFirmwareEnvironmentVariable = (
                kernel32.GetFirmwareEnvironmentVariableW
            )
            self._GetFirmwareEnvironmentVariable.restype = c_int
            self._GetFirmwareEnvironmentVariable.argtypes = [
                c_wchar_p,
                c_wchar_p,
                c_void_p,
                c_int,
            ]
            self._EnumerateFirmwareEnvironmentVariable = (
                windll.ntdll.NtEnumerateSystemEnvironmentValuesEx
            )
            self._EnumerateFirmwareEnvironmentVariable.restype = c_int
            self._EnumerateFirmwareEnvironmentVariable.argtypes = [
                c_int,
                c_void_p,
                c_void_p
            ]
            self._SetFirmwareEnvironmentVariable = (
                kernel32.SetFirmwareEnvironmentVariableW
            )
            self._SetFirmwareEnvironmentVariable.restype = c_int
            self._SetFirmwareEnvironmentVariable.argtypes = [
                c_wchar_p,
                c_wchar_p,
                c_void_p,
                c_int,
            ]
            self._SetFirmwareEnvironmentVariableEx = (
                kernel32.SetFirmwareEnvironmentVariableExW
            )
            self._SetFirmwareEnvironmentVariableEx.restype = c_int
            self._SetFirmwareEnvironmentVariableEx.argtypes = [
                c_wchar_p,
                c_wchar_p,
                c_void_p,
                c_int,
                c_int,
            ]
        except Exception:
            logging.warn(
                "G[S]etFirmwareEnvironmentVariableW function doesn't seem to exist"
            )
            pass

    #
    # Function to get variable
    # return a tuple of error code and variable data as string
    #
    def GetUefiVar(self, name, guid):
        # success
        err = 0
        efi_var = create_string_buffer(EFI_VAR_MAX_BUFFER_SIZE)
        if self._GetFirmwareEnvironmentVariable is not None:
            logging.info(
                "calling GetFirmwareEnvironmentVariable( name='%s', GUID='%s' ).."
                % (name, "{%s}" % guid)
            )
            length = self._GetFirmwareEnvironmentVariable(
                name, "{%s}" % guid, efi_var, EFI_VAR_MAX_BUFFER_SIZE
            )
        if (0 == length) or (efi_var is None):
            err = kernel32.GetLastError()
            if err != 0 and err != UefiVariable.ERROR_ENVVAR_NOT_FOUND:
                logging.error(
                    "GetFirmwareEnvironmentVariable[Ex] failed (GetLastError = 0x%x)" % err
                )
                logging.error(WinError())
            if efi_var is None:
                return (err, None)
        return (err, efi_var[:length])

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
        # From NTSTATUS definition
        STATUS_BUFFER_TOO_SMALL = 0xC0000023
        VARIABLE_INFORMATION_NAMES = 1
        # success
        length = DWORD(0)
        efi_var_names = create_string_buffer(length.value)
        if self._EnumerateFirmwareEnvironmentVariable is not None:
            logging.info(
                "calling _EnumerateFirmwareEnvironmentVariable to get size.."
            )
            status = self._EnumerateFirmwareEnvironmentVariable(
                VARIABLE_INFORMATION_NAMES, efi_var_names, pointer(length)
            )
            # Only inspect the lower 32bit.
            status = (0xFFFFFFFF & status)
            if status == STATUS_BUFFER_TOO_SMALL:
                logging.info(
                    "calling _EnumerateFirmwareEnvironmentVariable again to get data.."
                )
                efi_var_names = create_string_buffer(length.value)
                status = self._EnumerateFirmwareEnvironmentVariable(
                    VARIABLE_INFORMATION_NAMES, efi_var_names, pointer(length)
                )
        if (0 != status):
            logging.error(
                "EnumerateFirmwareEnvironmentVariable failed (GetLastError = 0x%x)" % status
            )
            return (status, None)
        return (status, efi_var_names)

    #
    # Function to set variable
    # return a tuple of boolean status, error_code, error_string (None if not error)
    #
    def SetUefiVar(self, name, guid, var=None, attrs=None):
        var_len = 0
        err = 0
        if var is None:
            var = bytes(0)
        else:
            var_len = len(var)
        success = 0  # Fail
        if attrs is None:
            if self._SetFirmwareEnvironmentVariable is not None:
                logging.info(
                    "Calling SetFirmwareEnvironmentVariable (name='%s', Guid='%s')..."
                    % (
                        name,
                        "{%s}" % guid,
                    )
                )
                success = self._SetFirmwareEnvironmentVariable(
                    name, "{%s}" % guid, var, var_len
                )
        else:
            attrs = int(attrs)
            if self._SetFirmwareEnvironmentVariableEx is not None:
                logging.info(
                    "Calling SetFirmwareEnvironmentVariableEx( name='%s', GUID='%s', length=0x%X, attributes=0x%X ).."
                    % (name, "{%s}" % guid, var_len, attrs)
                )
                success = self._SetFirmwareEnvironmentVariableEx(
                    name, "{%s}" % guid, var, var_len, attrs
                )

        if 0 == success:
            err = kernel32.GetLastError()
            logging.error(
                "SetFirmwareEnvironmentVariable failed (GetLastError = 0x%x)" % err
            )
            logging.error(WinError())
        return success
