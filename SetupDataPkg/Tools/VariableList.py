## @ GenCfgData.py
#
# Copyright (c) 2022, Microsoft Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import os
import sys
import re
import struct
import marshal
import pprint
import string
import operator as op
import ast
import binascii
from   datetime    import date
from   collections import OrderedDict
from xml.dom.minidom import parse, Node

from CommonUtility import *

class ParseError(Exception):
    def __init__(self, message):
        super().__init__(message)

def split_braces(string_object):
    segments = []
    depth = 0
    chars = []
    complete = False
    for c in string_object.strip():
        if complete and c == ' ':
            continue
        elif complete:
            raise ParseError("Unexpected character after '}}', found '{}'".format(c))

        if c == '{':
            if depth > 0:
                chars.append(c)
            depth += 1
        elif c == ',':
            if depth > 1:
                chars.append(c)
            else:
                segments.append("".join(chars).strip())
                chars = []
        elif c == '}':
            depth -= 1
            if depth > 0:
                chars.append(c)
            elif depth == 0:
                segments.append("".join(chars).strip())
                chars = []
                complete = True
        elif depth > 0:
            chars.append(c)
        else:
            raise ParseError("expected '{{', found '{}'".format(c))
    if not complete:
        ParseError("expected '}'")

    return segments

# A DataFormat defines how a variable element is serialized
# * to/from strings (as in the XML attributes) as well as
# * to/from binary (as in the variable list)
# * to C data types
class DataFormat:
    def __init__(self, ctype):
        self.ctype = ctype
        pass

# Represents all data types that have an object representation as a Python int
class IntValueFormat(DataFormat):
    def __init__(self, ctype, pack_format):
        super().__init__(ctype)
        self.pack_format = pack_format

    def string_to_object(self, string_representation):
        try:
            return int(string_representation)
        except:
            raise ParseError("Value '{}' is not a valid number".format(string_representation))

    def object_to_string(self, object_representation):
        return str(object_representation)
    def object_to_binary(self, object_representation):
        return struct.pack(self.pack_format, object_representation)
    def binary_to_object(self, binary_representation):
        return struct.unpack(self.pack_format, binary_representation)[0]
    def size_in_bytes(self):
        return struct.calcsize(self.pack_format)

# Represents all data types that have an object representation as a Python float
class FloatValueFormat(DataFormat):
    def __init__(self, ctype, pack_format):
        super().__init__(ctype)
        self.pack_format = pack_format

    def string_to_object(self, string_representation):
        return float(string_representation)
    def object_to_string(self, object_representation):
        return float(object_representation)
    def object_to_binary(self, object_representation):
        return struct.pack(self.pack_format, object_representation)
    def binary_to_object(self, binary_representation):
        return struct.unpack(self.pack_format, binary_representation)[0]
    def size_in_bytes(self):
        return struct.calcsize(self.pack_format)

# Represents all data types that have an object representation as a Python bool
class BoolFormat(DataFormat):
    def __init__(self, ctype, pack_format):
        super().__init__(ctype='bool')

    def string_to_object(self, string_representation):
        return string_representation.strip().lower() in ['true', '1', 'yes']
    def object_to_string(self, object_representation):
        if object_representation:
            return 'true'
        else:
            return 'false'
    def object_to_binary(self, object_representation):
        return struct.pack("<?", object_representation)
    def binary_to_object(self, binary_representation):
        return struct.unpack("<?", binary_representation)[0]
    def size_in_bytes(self):
        return struct.calcsize("<?")


builtin_types = {
    'uint8_t'  : (lambda: IntValueFormat(ctype='uint8_t', pack_format='<B')),
    'int8_t'   : (lambda: IntValueFormat(ctype='int8_t', pack_format='<b')),
    'uint16_t' : (lambda: IntValueFormat(ctype='uint16_t', pack_format='<H')),
    'int16_t'  : (lambda: IntValueFormat(ctype='int16_t', pack_format='<h')),
    'uint32_t' : (lambda: IntValueFormat(ctype='uint32_t', pack_format='<I')),
    'int32_t'  : (lambda: IntValueFormat(ctype='int32_t', pack_format='<i')),
    'uint64_t' : (lambda: IntValueFormat(ctype='uint64_t', pack_format='<Q')),
    'int64_t'  : (lambda: IntValueFormat(ctype='int64_t', pack_format='<q')),
    'float'    : (lambda: FloatValueFormat(ctype='float', pack_format='<f')),
    'double'   : (lambda: FloatValueFormat(ctype='double', pack_format='<d')),
    'bool'     : (lambda: BoolFormat()),
}

class EnumValue:
    def __init__(self, enum_name, xml_node):
        self.enum_name = enum_name
        self.name = xml_node.getAttribute("name")
        self.number = int(xml_node.getAttribute("value"))
        self.help = xml_node.getAttribute("help")

    def generate_header(self, out):
        if self.help != "":
            out.write("    // {}\n".format(self.help))
        if self.number != "":
            out.write("    {}_{} = {},\n".format(self.enum_name, self.name, self.number))
        else:
            out.write("    {}_{},\n".format(self.enum_name, self.name))

class EnumFormat(DataFormat):
    def __init__(self, xml_node):
        self.name = xml_node.getAttribute("name")
        self.help = xml_node.getAttribute("help")
        self.values = []

        
        super().__init__(ctype=self.name)

        for value in xml_node.getElementsByTagName('Value'):
            self.values.append(EnumValue(self.name, value))
        pass

    def generate_header(self, out):
        if self.help != "":
            out.write("// {}\n".format(self.help))

        out.write("enum {} {{\n".format(self.name))
        for value in self.values:
            value.generate_header(out)
        out.write("};\n")
        out.write("\n")

    def string_to_object(self, string_representation):
        try:
            return int(string_representation)
        except:
            for value in self.values:
                if value.name == string_representation:
                    return value.number
        raise ParseError("Value '{}' is not a valid value of enum '{}'".format(string_representation, self.name))
    def object_to_string(self, object_representation):
        for value in self.values:
            if value.number == object_representation:
                return value.name
        return str(object_representation)
    def object_to_binary(self, object_representation):
        return struct.pack("<i", object_representation)
    def binary_to_object(self, binary_representation):
        return struct.unpack("<i", binary_representation)[0]
    def size_in_bytes(self):
        return struct.calcsize("<i")

class StructMember:
    def __init__(self, schema, xml_node):
        self.name = xml_node.getAttribute("name")
        self.help = xml_node.getAttribute("help")
        count = xml_node.getAttribute("count")
        if count == "":
            self.count = 1
        else:
            self.count = int(count)

        data_type = xml_node.getAttribute("type")
        # Look up the format using the schema
        #  The format may be a built-in format (e.g. 'uint32_t') or a user defined enum or struct
        self.format = schema.get_format(data_type)

    def generate_header(self, out):
        if self.help != "":
            out.write("    // {}\n".format(self.help))
        if self.count == "":
            out.write("    {} {};\n".format(self.format.ctype, self.name))
        else:
            out.write("    {} {}[{}];\n".format(self.format.ctype, self.name, self.count))

    # TODO: Handle arrays
    def string_to_object(self, string_representation):
        if self.count != 1:
            segments = split_braces(string_representation)
            object_array = []
            for element in segments:
                object_array.append(self.format.string_to_object(element))
            return object_array
        else:
            return self.format.string_to_object(string_representation)
    def object_to_string(self, object_representation):
        if self.count == 1:
            return self.format.object_to_string(object_representation)
        else:
            element_strings = []
            for element in object_representation:
                element_strings.append(self.format.object_to_string(element))
            return "{{{}}}".format(",".join(element_strings))

    def object_to_binary(self, object_representation):
        return self.format.object_to_binary(object_representation)
    def binary_to_object(self, binary_representation):
        return self.format.binary_to_object(binary_representation)
    def size_in_bytes(self):
        return self.format.size_in_bytes()

class StructFormat(DataFormat):
    def __init__(self, schema, xml_node):
        self.name = xml_node.getAttribute("name")
        self.help = xml_node.getAttribute("help")
        self.members = []

        super().__init__(ctype=self.name)

        for member in xml_node.getElementsByTagName('Member'):
            self.members.append(StructMember(schema, member))

        pass

    def generate_header(self, out):
        if self.help != "":
            out.write("// {}\n".format(self.help))

        out.write("struct {} {{\n".format(self.name))
        for member in self.members:
            member.generate_header(out)
        out.write("};\n")
        out.write("\n")

    def string_to_object(self, string_representation):
        obj = OrderedDict()

        segments = split_braces(string_representation)
            
        for (member, value) in zip(self.members, segments):
            obj[member.name] = member.string_to_object(value)
        return obj
        
    def object_to_string(self, object_representation):
        member_strings = []

        for member in self.members:
            value = object_representation[member.name]
            member_strings.append(member.object_to_string(value))

        return "{{{}}}".format(",".join(member_strings))
    def object_to_binary(self, object_representation):
        binary_representation = b''
        for member in self.members:
            value = object_representation[member.name]
            member_binary = member.object_to_binary(value)
            binary_representation += member_binary
            assert len(member_binary) == member.size_in_bytes()
        return binary_representation
    def binary_to_object(self, binary_representation):
        raise Exception("not implemented")
    def size_in_bytes(self):
        total_size = 0
        for member in self.members:
            total_size += member.size_in_bytes()
        return total_size

class Knob:
    def __init__(self, schema, xml_node, namespace):
        self.name = xml_node.getAttribute("name")
        self.help = xml_node.getAttribute("help")
        self.namespace = namespace

        data_type = xml_node.getAttribute("type")
        # Look up the format using the schema
        #  The format may be a built-in format (e.g. 'uint32_t') or a user defined enum or struct
        self.format = schema.get_format(data_type)

        # Use the format to decode the default value from its string representation
        try:
            self.default = self.format.string_to_object(xml_node.getAttribute("default"))
        except ParseError as e:
            raise Exception("Unable to parse default value of '{}': {}".format(self.name, e))

        print("Knob '{}', default='{}'".format(self.name, self.format.object_to_string(self.default)))
        pass


class Schema:
    def __init__(self, path):
        dom = parse(path)

        self.enums = []
        self.structs = []
        self.knobs = []

        for section in dom.getElementsByTagName('Enums'):
            for enum in section.getElementsByTagName('Enum'):
                self.enums.append(EnumFormat(enum))

        for section in dom.getElementsByTagName('Structs'):
            for struct in section.getElementsByTagName('Struct'):
                self.structs.append(StructFormat(self, struct))

        for section in dom.getElementsByTagName('Knobs'):
            namespace = section.getAttribute('namespace')
            for knob in section.getElementsByTagName('Knob'):
                self.knobs.append(Knob(self, knob, namespace))
        pass

    def get_format(self, type_name):

        # Look for the type in the set of built in types
        if type_name in builtin_types:
            # Construct a new instance of the built in type format
            return builtin_types[type_name]()

        # Check for custom enum or struct types

        for enum in self.enums:
            if enum.name == type_name:
                return enum

        for struct in self.structs:
            if struct.name == type_name:
                return struct
        

        raise Exception("Data type '{}' is not defined".format(type_name))


def generate_header(schema_path, header_path):
    
    schema = Schema(schema_path)
    
    with open(header_path, 'w') as out:
        out.write("#pragma once\n")
        out.write("// Generated Header\n")
        out.write("// Script: {}\n".format(sys.argv[0]))
        out.write("// Schema: {}\n".format(schema_path))
        out.write("\n")
        out.write("// config_common.h includes helper functions for the generated interfaces that")
        out.write("// do not require code generation themselves")
        out.write("#include \"config_common.h\"\n")
        out.write("\n")
        for enum in schema.enums:
            enum.generate_header(out)
            pass
        for struct in schema.structs:
            struct.generate_header(out)
            pass
        for knob in schema.knobs:
            pass

    
    pass

def generate_vlist(schema_path, values_path):
    pass

def generate_valuesxml(schema_path, vlist_path):
    pass


def usage():
    print("Commands:\n")
    print("  generateheader <schema.xml> <header.h>")

def main():

    if len(sys.argv) < 2:
        usage()
        sys.stderr.write('Must provide a command.\n')
        sys.exit(1)

    if sys.argv[1].lower() == "generateheader":
        if len(sys.argv) < 2:
            usage()
            sys.stderr.write('Must provide a command.\n')
            sys.exit(1)

        schema_path = sys.argv[2]
        header_path = sys.argv[3]

        generate_header(schema_path, header_path)
    
if __name__ == '__main__':
    sys.exit(main())

#  schema.xml values.xml -> blob.vl
#  schema.xml blob.vl -> values.xml
#  schema.xml -> config.h
#  blob.vl -> partition.bin