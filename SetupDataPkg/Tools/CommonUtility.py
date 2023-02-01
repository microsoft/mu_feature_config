#!/usr/bin/env python
## @ CommonUtility.py
# Common utility script
#
# Copyright (c) 2016 - 2020, Intel Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

##
# Import Modules
#
import string


def print_bytes(data, indent=0, offset=0, show_ascii=False):
    bytes_per_line = 16
    printable = ' ' + string.ascii_letters + string.digits + string.punctuation
    str_fmt = '{:s}{:04x}: {:%ds} {:s}' % (bytes_per_line * 3)
    bytes_per_line
    data_array = bytearray(data)
    for idx in range(0, len(data_array), bytes_per_line):
        hex_str = ' '.join('%02X' % val for val in data_array[idx:idx + bytes_per_line])
        asc_str = ''.join('%c' % (val if (chr(val) in printable) else '.')
                          for val in data_array[idx:idx + bytes_per_line])
        print(str_fmt.format(indent * ' ', offset + idx, hex_str, ' ' + asc_str if show_ascii else ''))


def get_bits_from_bytes(bytes, start, length):
    if length == 0:
        return 0
    byte_start = (start) // 8
    byte_end = (start + length - 1) // 8
    bit_start = start & 7
    mask = (1 << length) - 1
    val = bytes_to_value(bytes[byte_start:byte_end + 1])
    val = (val >> bit_start) & mask
    return val


def set_bits_to_bytes(bytes, start, length, bvalue):
    if length == 0:
        return
    byte_start = (start) // 8
    byte_end = (start + length - 1) // 8
    bit_start = start & 7
    mask = (1 << length) - 1
    val = bytes_to_value(bytes[byte_start:byte_end + 1])
    val &= ~(mask << bit_start)
    val |= ((bvalue & mask) << bit_start)
    bytes[byte_start:byte_end + 1] = value_to_bytearray(val, byte_end + 1 - byte_start)


def check_quote(text):
    if (text[0] == "'" and text[-1] == "'") or (text[0] == '"' and text[-1] == '"'):
        return True
    return False


def strip_quote(text):
    new_text = text.strip()
    if check_quote(new_text):
        return new_text[1:-1]
    return text


def strip_delimiter(text, delim):
    new_text = text.strip()
    if new_text:
        if new_text[0] == delim[0] and new_text[-1] == delim[-1]:
            return new_text[1:-1]
    return text


def value_to_bytes(value, length):
    return value.to_bytes(length, 'little')


def bytes_to_value(bytes):
    return int.from_bytes(bytes, 'little')


def value_to_bytearray(value, length):
    return bytearray(value_to_bytes(value, length))


def bytes_to_bracket_str(bytes):
    return '{ %s }' % (', '.join('0x%02x' % i for i in bytes))


def array_str_to_value(val_str):
    val_str = val_str.strip()
    val_str = strip_delimiter(val_str, '{}')
    val_str = strip_quote(val_str)
    value = 0
    for each in val_str.split(',')[::-1]:
        each = each.strip()
        value = (value << 8) | int(each, 0)
    return value
