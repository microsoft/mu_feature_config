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
import os
import string
from importlib.machinery import SourceFileLoader


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
    #print("OSDDEBUG bits_from_bytes bytes: ", bytes, "\nstart: ", start, "\nlength: ", length)
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


def value_to_bytes(value, length):
    return value.to_bytes(length, 'little')


def bytes_to_value(bytes):
    return int.from_bytes(bytes, 'little')


def value_to_bytearray(value, length):
    return bytearray(value_to_bytes(value, length))


def get_aligned_value(value, alignment=4):
    if alignment != (1 << (alignment.bit_length() - 1)):
        raise Exception('Alignment (0x%x) should to be power of 2 !' % alignment)
    value = (value + (alignment - 1)) & ~(alignment - 1)
    return value


def get_padding_length(data_len, alignment=4):
    new_data_len = get_aligned_value(data_len, alignment)
    return new_data_len - data_len


def get_file_data(file, mode='rb'):
    return open(file, mode).read()


def gen_file_from_object(file, object):
    open(file, 'wb').write(object)


def gen_file_with_size(file, size):
    open(file, 'wb').write(b'\xFF' * size)


def check_files_exist(base_name_list, dir='', ext=''):
    for each in base_name_list:
        if not os.path.exists(os.path.join(dir, each + ext)):
            return False
    return True


def load_source(name, filepath):
    mod = SourceFileLoader(name, filepath).load_module()
    return mod
