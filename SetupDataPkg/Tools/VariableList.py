# @ VariableList.py
#
# Copyright (c) 2022, Microsoft Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

import sys
import re
import struct
import csv
from collections import OrderedDict
import uuid
import zlib
import copy
from xml.dom.minidom import parse, parseString


class ParseError(Exception):
    def __init__(self, message):
        super().__init__(message)


class InvalidNameError(ParseError):
    def __init__(self, message):
        super().__init__(message)


class InvalidTypeError(ParseError):
    def __init__(self, message):
        super().__init__(message)


class InvalidKnobError(ParseError):
    def __init__(self, message):
        super().__init__(message)


# Decodes a comma sepearated list within curly braces in to a list
# "{1,2, 3,{A, B}} " -> ["1", "2", "3", "{A, B}"]
def split_braces(string_object):
    segments = []
    depth = 0
    chars = []
    complete = False
    for c in string_object.strip():
        if complete and c == ' ':
            continue
        elif complete:
            raise ParseError(
                "Unexpected character after '}}', found '{}'".format(
                    c))

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


# Checks to see if a token is a valid C identifier
def is_valid_name(token):
    matches = re.findall("^[a-zA-Z_][0-9a-zA-Z_]*$", token)
    return len(matches) == 1


# This class can be used to modify the behavior of string formatting
#  of variable values
class StringFormatOptions:
    def __init__(self):
        pass

    # When true, format the values for use in "C"
    # When false, format the values for use in the XML
    cformat = False


# A DataFormat defines how a variable element is serialized
# * to/from strings (as in the XML attributes) as well as
# * to/from binary (as in the variable list)
# * to C data types
class DataFormat:
    def __init__(self, ctype):
        self.ctype = ctype
        pass


# Represents all data types that have an object representation as a
# Python int
class IntValueFormat(DataFormat):
    def __init__(self, ctype, pack_format, csuffix=""):
        super().__init__(ctype)
        self.pack_format = pack_format
        self.csuffix = csuffix
        self.default = 0

    def string_to_object(self, string_representation):
        try:
            return int(string_representation, 0)
        except ValueError:
            raise ParseError(
                "Value '{}' is not a valid number".format(
                    string_representation))

    def object_to_string(
            self,
            object_representation,
            options=StringFormatOptions()):
        if options.cformat:
            return "{}{}".format(object_representation, self.csuffix)
        else:
            return str(object_representation)

    def object_to_binary(self, object_representation):
        return struct.pack(self.pack_format, object_representation)

    def binary_to_object(self, binary_representation):
        return struct.unpack(self.pack_format, binary_representation)[0]

    def size_in_bytes(self):
        return struct.calcsize(self.pack_format)


# Represents all data types that have an object representation as a
# Python float
class FloatValueFormat(DataFormat):
    def __init__(
            self,
            ctype,
            pack_format,
            csuffix=""):
        super().__init__(ctype)
        self.pack_format = pack_format
        self.csuffix = csuffix
        self.default = 0.0

    def string_to_object(self, string_representation):
        return float(string_representation)

    def object_to_string(
            self,
            object_representation,
            options=StringFormatOptions()):
        if options.cformat:
            return "{}{}".format(object_representation, self.csuffix)
        else:
            return str(object_representation)

    def object_to_binary(self, object_representation):
        return struct.pack(self.pack_format, object_representation)

    def binary_to_object(self, binary_representation):
        return struct.unpack(self.pack_format, binary_representation)[0]

    def size_in_bytes(self):
        return struct.calcsize(self.pack_format)


# Represents all data types that have an object representation as a Python bool
class BoolFormat(DataFormat):
    def __init__(self):
        super().__init__(ctype='bool')
        self.default = False

    def string_to_object(self, string_representation):
        return string_representation.strip().lower() in ['true', '1', 'yes']

    def object_to_string(
            self,
            object_representation,
            options=StringFormatOptions()):
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
    'uint8_t'  : (lambda: IntValueFormat(ctype='uint8_t', pack_format='<B')),                   # noqa: E203, E501
    'int8_t'   : (lambda: IntValueFormat(ctype='int8_t', pack_format='<b')),                    # noqa: E203, E501
    'uint16_t' : (lambda: IntValueFormat(ctype='uint16_t', pack_format='<H')),                  # noqa: E203, E501
    'int16_t'  : (lambda: IntValueFormat(ctype='int16_t', pack_format='<h')),                   # noqa: E203, E501
    'uint32_t' : (lambda: IntValueFormat(ctype='uint32_t', pack_format='<I', csuffix="ul")),    # noqa: E203, E501
    'int32_t'  : (lambda: IntValueFormat(ctype='int32_t', pack_format='<i', csuffix="l")),      # noqa: E203, E501
    'uint64_t' : (lambda: IntValueFormat(ctype='uint64_t', pack_format='<Q', csuffix="ull")),   # noqa: E203, E501
    'int64_t'  : (lambda: IntValueFormat(ctype='int64_t', pack_format='<q', csuffix="ll")),     # noqa: E203, E501
    'float'    : (lambda: FloatValueFormat(ctype='float', pack_format='<f', csuffix="f")),      # noqa: E203, E501
    'double'   : (lambda: FloatValueFormat(ctype='double', pack_format='<d')),                  # noqa: E203, E501
    'bool'     : (lambda: BoolFormat()),                                                        # noqa: E203, E501
}


# Represents a value in a user defined enum
class EnumValue:
    def __init__(self, enum_name, xml_node):
        self.enum_name = enum_name
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError(
                "Enum '{}' has invalid value name '{}'".format(
                    enum_name,
                    self.name))

        try:
            self.number = int(xml_node.getAttribute("value"))
        except ValueError:
            raise ParseError(
                "Enum '{}' value '{}' does not have a valid value number"
                .format(
                    enum_name,
                    self.name))

        self.help = xml_node.getAttribute("help")


# Represents a user defined enum
class EnumFormat(DataFormat):
    def __init__(self, xml_node):
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError(
                "Enum name '{}' is invalid".format(self.name))

        self.help = xml_node.getAttribute("help")
        self.values = []

        super().__init__(ctype=self.name)

        for value in xml_node.getElementsByTagName('Value'):
            self.values.append(EnumValue(self.name, value))
        pass

    def string_to_object(self, string_representation):
        try:
            return int(string_representation)
        except ValueError:
            for value in self.values:
                if value.name == string_representation:
                    return value.number
        raise ParseError(
            "Value '{}' is not a valid value of enum '{}'".format(
                string_representation,
                self.name))

    def object_to_string(
            self,
            object_representation,
            options=StringFormatOptions()):
        for value in self.values:
            if value.number == object_representation:
                if options.cformat:
                    return "{}_{}".format(self.name, value.name)
                else:
                    return value.name
        return str(object_representation)

    def object_to_binary(self, object_representation):
        return struct.pack("<i", object_representation)

    def binary_to_object(self, binary_representation):
        return struct.unpack("<i", binary_representation)[0]

    def size_in_bytes(self):
        return struct.calcsize("<i")


# Represents a user defined struct
class ArrayFormat(DataFormat):
    def __init__(self, child_format, struct_name, member_name, count):
        self.format = child_format
        self.count = count
        self.struct_name = struct_name
        self.member_name = member_name

        super().__init__(ctype=child_format.ctype)

        self.default = []
        for i in range(self.count):
            self.default.append(self.format.default)
        pass

    def string_to_object(self, string_representation):
        segments = split_braces(string_representation)
        if len(segments) != self.count:
            raise ParseError(
                "Member '{}' of struct '{}' should have {} elements, "
                "but {} were found"
                .format(
                    self.member_name,
                    self.struct_name,
                    self.count,
                    len(segments)))

        object_array = []
        for element in segments:
            object_array.append(self.format.string_to_object(element))
        return object_array

    def object_to_string(
            self,
            object_representation,
            options=StringFormatOptions()):
        element_strings = []
        for element in object_representation:
            element_strings.append(
                self.format.object_to_string(element, options))

        return "{{{}}}".format(",".join(element_strings))

    def object_to_binary(self, object_representation):
        binary = b''
        for element in object_representation:
            binary += self.format.object_to_binary(element)
        return binary

    def binary_to_object(self, binary_representation):
        values = []
        element_size = self.format.size_in_bytes()
        for i in range(self.count):
            element_binary = binary_representation[
                (i * element_size):((i+1) * element_size)]
            values.append(self.format.binary_to_object(element_binary))
        return values

    def size_in_bytes(self):
        return self.format.size_in_bytes() * self.count


# Represents a member of a user defined struct
# Each member may be of any type, and maay also have a count
# value to treat it as an array
class StructMember:
    def __init__(self, schema, struct_name, xml_node):
        self.name = xml_node.getAttribute("name")
        self.struct_name = struct_name
        if not is_valid_name(self.name):
            raise InvalidNameError(
                "Member name '{}' of struct '{}' is invalid".format(
                    self.name,
                    self.struct_name))
        self.help = xml_node.getAttribute("help")
        count = xml_node.getAttribute("count")
        if count == "":
            self.count = 1
        else:
            self.count = int(count)

        data_type = xml_node.getAttribute("type")
        # Look up the format using the schema
        # The format may be a built-in format (e.g. 'uint32_t') or a user
        # defined enum or struct
        format = schema.get_format(data_type)

        if self.count != 1:
            self.format = ArrayFormat(
                format,
                struct_name,
                self.name,
                self.count)
        else:
            self.format = format

        default_string = xml_node.getAttribute("default")
        if default_string == "":
            self.default = self.format.default
        else:
            self.default = self.format.string_to_object(default_string)

    def string_to_object(self, string_representation):
        return self.format.string_to_object(string_representation)

    def object_to_string(
            self,
            object_representation,
            options=StringFormatOptions()):
        return self.format.object_to_string(object_representation, options)

    def object_to_binary(self, object_representation):
        return self.format.object_to_binary(object_representation)

    def binary_to_object(self, binary_representation):
        return self.format.binary_to_object(binary_representation)

    def size_in_bytes(self):
        return self.format.size_in_bytes()


# Represents a user defined struct
class StructFormat(DataFormat):
    def __init__(self, schema, xml_node):
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError("Struct name '{}' is invalid".format(
                self.name))
        self.help = xml_node.getAttribute("help")
        self.members = []

        super().__init__(ctype=self.name)

        for member in xml_node.getElementsByTagName('Member'):
            self.members.append(StructMember(schema, self.name, member))

        self.default = self.string_to_object(xml_node.getAttribute("default"))

        pass

    def create_subknobs(self, knob, path):
        subknobs = []
        subknobs.append(SubKnob(knob, path, self, self.help))
        for member in self.members:
            subpath = "{}.{}".format(path, member.name)
            if member.count == 1:
                if isinstance(member.format, StructFormat):
                    subknobs += member.format.create_subknobs(knob, subpath)
                else:
                    subknobs.append(
                        SubKnob(
                            knob,
                            subpath,
                            member.format,
                            member.help,
                            True  # Leaf
                            ))
            else:
                subknobs.append(
                    SubKnob(
                        knob,
                        subpath,
                        member.format,
                        member.help,
                        False  # Leaf
                        ))

                for i in range(member.count):
                    indexed_subpath = "{}.{}[{}]".format(path, member.name, i)
                    if isinstance(member.format.format, StructFormat):
                        subknobs += member.format.format.create_subknobs(
                            knob,
                            indexed_subpath)
                    else:
                        subknobs.append(
                            SubKnob(
                                knob,
                                indexed_subpath,
                                member.format.format,
                                member.help,
                                True  # Leaf
                                ))
        return subknobs

    def string_to_object(self, string_representation):
        obj = OrderedDict()

        segments = split_braces(string_representation)

        # Get the defaults from the member values
        if len(segments) == 0 or (len(segments) == 1 and segments[0] == ""):
            for member in self.members:
                obj[member.name] = member.default
            return obj

        if len(self.members) > len(segments):
            raise ParseError(
                "Value '{}' does not have enough members for struct '{}'"
                .format(
                    string_representation,
                    self.name))
        if len(self.members) < len(segments):
            raise ParseError(
                "Value '{}' does has too many members for struct '{}'"
                .format(
                    string_representation,
                    self.name))

        for (member, value) in zip(self.members, segments):
            obj[member.name] = member.string_to_object(value)
        return obj

    def object_to_string(
            self,
            object_representation,
            options=StringFormatOptions()):
        member_strings = []

        for member in self.members:
            value = object_representation[member.name]
            if options.cformat:
                member_strings.append(".{}={}".format(
                    member.name,
                    member.object_to_string(value, options)))
            else:
                member_strings.append(member.object_to_string(value, options))

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
        position = 0
        obj = OrderedDict()
        for member in self.members:
            member_binary = binary_representation[
                position:(position+member.size_in_bytes())]
            member_value = member.binary_to_object(member_binary)
            obj[member.name] = member_value
            position += member.size_in_bytes()
        return obj

    def size_in_bytes(self):
        total_size = 0
        for member in self.members:
            total_size += member.size_in_bytes()
        return total_size


# Represents a "knob", a modifiable variable
class Knob:
    def __init__(self, schema, xml_node, namespace):
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError(
                "Knob name '{}' is invalid".format(self.name))
        self.help = xml_node.getAttribute("help")
        self.namespace = namespace

        data_type = xml_node.getAttribute("type")
        if data_type == "":
            raise ParseError(
                "Knob '{}' does not have a type".format(self.name))

        # Look up the format using the schema
        # The format may be a built-in format (e.g. 'uint32_t') or a
        # user defined enum or struct
        self.format = schema.get_format(data_type)

        self.value = None

        default_string = xml_node.getAttribute("default")
        if default_string == "":
            self._default = self.format.default
        else:
            # Use the format to decode the default value from its
            # string representation
            try:
                self._default = self.format.string_to_object(default_string)
            except ParseError as e:
                # Capture the ParseError and create a more specific error
                # message
                raise ParseError(
                    "Unable to parse default value of '{}': {}".format(
                        self.name,
                        e))

        self.subknobs = []
        if isinstance(self.format, StructFormat):
            self.subknobs.append(
                SubKnob(
                    self,
                    self.name,
                    self.format,
                    self.help))
            self.subknobs += self.format.create_subknobs(self, self.name)
        else:
            self.subknobs.append(
                SubKnob(
                    self,
                    self.name,
                    self.format,
                    self.help,
                    True  # Leaf
                    ))

        pass

    # Get the default value for this knob
    @property
    def default(self):
        # Return as deep copy so that modifications to the returned
        # object don't affect the default value of the knob
        return copy.deepcopy(self._default)

    def generate_header(self, out):
        out.write("// {} knob\n".format(self.name))
        if self.help != "":
            out.write("// {}\n".format(self.help))

        format_options = StringFormatOptions()
        format_options.cformat = True

        out.write("// Namspace GUID for {}\n".format(self.name))
        out.write("#define CONFIG_NAMESPACE_{} \"{}\"\n\n".format(self.name, self.namespace))  # noqa: E501

        out.write("// Get the default value of {}\n".format(self.name))
        out.write("inline {} config_get_default_{}() {{\n".format(self.format.ctype, self.name))  # noqa: E501
        out.write("    return {};\n".format(self.format.object_to_string(self.default, format_options)))  # noqa: E501
        out.write("}\n")
        out.write("// Get the current value of {}\n".format(self.name))
        out.write("inline {} config_get_{}() {{\n".format(self.format.ctype, self.name))  # noqa: E501
        out.write("    {} result = config_get_default_{}();\n".format(self.format.ctype, self.name))  # noqa: E501
        out.write("    size_t value_size = sizeof(result);\n")  # noqa: E501
        out.write("    config_result_t read_result = _config_get_knob_value_by_name(\"{}\", CONFIG_NAMESPACE_{}, &value_size, (unsigned char*)&result);\n".format(self.name, self.name))  # noqa: E501
        out.write("    // i.e. an enum may be stored as a single byte, or four bytes; it may use however many bytes are needed\n")  # noqa: E501
        out.write("    if (read_result == CONFIG_RESULT_SUCCESS && value_size == sizeof(read_result)) {\n")  # noqa: E501
        out.write("        return result;\n")
        out.write("    } else {\n")
        out.write("        return config_get_default_{}();\n".format(self.name))  # noqa: E501
        out.write("    }\n")
        out.write("}\n")
        out.write("#ifdef CONFIG_SET_VALUES\n")
        out.write("// Set the current value of {}\n".format(self.name))
        out.write("inline config_result_t config_set_{}({} value) {{\n".format(self.name, self.format.ctype))  # noqa: E501
        out.write("    return _config_set_knob_value_by_name(\"{}\", CONFIG_NAMESPACE_{}, sizeof(value), (unsigned char*)&value);\n".format(self.name, self.name))  # noqa: E501
        out.write("}\n")
        out.write("#endif\n")
        out.write("\n\n")

    def _get_child_value(self, child_path, base_value):
        path_elements = child_path.split(".")
        child_object = base_value

        first_element = path_elements[0]
        if first_element != self.name:
            raise Exception(
                "Path '{}' is not a member of knob '{}'".format(
                    child_path,
                    self.name))

        for element in path_elements[1:]:
            (name, index) = self._decode_subpath(element)
            if index is None:
                child_object = child_object[name]
            else:
                child_object = child_object[name][index]
        return child_object

    def _set_child_value(self, child_path, value):
        if child_path == self.name:
            self.value = value
        else:
            path_elements = child_path.split(".")
            child_object = self.value

            if child_object is None:
                child_object = self.default

            first_element = path_elements[0]
            if first_element != self.name:
                raise Exception(
                    "Path '{}' is not a member of knob '{}'".format(
                        child_path,
                        self.name))

            # The final element will reference a value type, not an object,
            # so it needs to be handled differently
            final_element = path_elements[-1]

            # Iterate the path until we find the object
            for element in path_elements[1:-1]:
                (name, index) = self._decode_subpath(element)
                if index is None:
                    child_object = child_object[name]
                else:
                    child_object = child_object[name][index]

            (name, index) = self._decode_subpath(final_element)
            if index is None:
                child_object[name] = value
            else:
                child_object[name][index] = value

    def _decode_subpath(self, subpath_segment):
        match = re.match(
            r'^(?P<name>[a-zA-Z_][0-9a-zA-Z_]*)(\[(?P<index>[0-9]+)\])?$',
            subpath_segment)

        if match is None:
            raise ParseError(
                "'{}' is not a valid sub-path of knob '{}'".format(
                    subpath_segment,
                    self.name))

        if match.group('index') is not None:  # An index was included
            name = match.group('name')
            index = int(match.group('index'))
            return (name, index)
        else:
            name = match.group('name')
            return (name, None)


class SubKnob:
    def __init__(self, knob, path, format, help, leaf=False):
        self.knob = knob
        self.name = path
        self.format = format
        self.help = help
        self.leaf = leaf
        pass

    @property
    def value(self):
        return self.knob._get_child_value(self.name, self.knob.value)

    @value.setter
    def value(self, value):
        if self.knob.value is None:
            self.knob.value = self.knob.default

        self.knob._set_child_value(self.name, value)
        pass

    @property
    def default(self):
        return self.knob._get_child_value(self.name, self.knob.default)


class Schema:
    def __init__(self, dom, origin_path = ""):
        self.enums = []
        self.structs = []
        self.knobs = []
        self.path = origin_path

        for section in dom.getElementsByTagName('Enums'):
            for enum in section.getElementsByTagName('Enum'):
                self.enums.append(EnumFormat(enum))

        for section in dom.getElementsByTagName('Structs'):
            for structnode in section.getElementsByTagName('Struct'):
                self.structs.append(StructFormat(self, structnode))

        for section in dom.getElementsByTagName('Knobs'):
            namespace = section.getAttribute('namespace')
            for knob in section.getElementsByTagName('Knob'):
                self.knobs.append(Knob(self, knob, namespace))

        self.subknobs = []
        for knob in self.knobs:
            self.subknobs += knob.subknobs

        pass

    # Load a schema given a path to a schema xml file
    def load(path):
        return Schema(parse(path), path)

    # Parse a schema given a string representation of the xml content
    def parse(string):
        return Schema(parseString(string))

    # Get a knob by name
    def get_knob(self, knob_name):
        for knob in self.subknobs:
            if knob.name == knob_name:
                return knob

        raise InvalidKnobError("Knob '{}' is not defined".format(knob_name))

    # Get a format by name
    def get_format(self, type_name):

        type_name = type_name.strip()

        # Look for the type in the set of built in types
        if type_name in builtin_types:
            # Construct a new instance of the built in type format
            return builtin_types[type_name]()

        # Check for custom enum or struct types

        for enum in self.enums:
            if enum.name == type_name:
                return enum

        for struct_definition in self.structs:
            if struct_definition.name == type_name:
                return struct_definition

        raise InvalidTypeError(
            "Data type '{}' is not defined".format(type_name))


# Represents a UEFI variable
# Knobs are stored within UEFI variables and can be serialized to a
# variable list file
class UEFIVariable:
    def __init__(self, name, guid, data, attributes=7):
        self.name = name
        if isinstance(guid, uuid.UUID):
            self.guid = guid
        else:
            self.guid = uuid.UUID(guid)
        self.attributes = attributes
        self.data = data


# Writes a vlist entry to the vlist file
def create_vlist_buffer(variable):

    # Each entry has the following values
    #   NameSize(int32, size of Name in bytes),
    #   DataSize(int32, size of Data in bytes),
    #   Name(null terminated UTF-16LE encoded name),
    #   Guid(16 Byte namespace Guid),
    #   Attributes(int32 UEFI attributes),
    #   Data(bytes),
    #   CRC32(int32 checksum of all bytyes from NameSize through Data)

    # Null terminated UTF-16LE encoded name
    name = (variable.name + "\0").encode("utf-16le")
    data = variable.data
    guid = variable.guid.bytes
    attributes = variable.attributes

    payload = struct.pack("<ii", len(name), len(data)) + \
        name + \
        guid + \
        struct.pack("<I", attributes) + \
        data

    crc = zlib.crc32(payload)

    return payload + struct.pack("<I", crc)


# Create a byte array for all the knobs in this schema
def binarize_vlist(schema):

    ret = b''
    for knob in schema.knobs:
        if knob.value is not None:
            value_bytes = knob.format.object_to_binary(knob.value)

            variable = UEFIVariable(knob.name, knob.namespace, value_bytes)
            ret += create_vlist_buffer(variable)

    return ret


# Read a set of UEFIVariables from a variable list file
def read_vlist(file):
    with open(file, 'rb') as vl_file:
        variables = read_vlist_from_buffer(vl_file.read())

    return variables


# Read a set of UEFIVariables from a variable list buffer
def read_vlist_from_buffer(array):
    variables = []
    temp_arr = array
    while len(temp_arr):
        # Try reading the first part
        name_size_bytes = temp_arr[:4]
        temp_arr = temp_arr[4:]

        # Decode the name size and read the data size
        name_size = struct.unpack("<i", name_size_bytes)[0]
        data_size = struct.unpack("<i", temp_arr[:4])[0]
        temp_arr = temp_arr[4:]

        # These portions are fixed size
        guid_size = 16
        attr_size = 4

        # This is the number of bytes *after* the two size integers
        # *until before* the CRC
        remaining_bytes = name_size + guid_size + attr_size + data_size

        # Payload will now contain all of the bytes including the size
        # bytes, but not the CRC
        payload = struct.pack("<ii", name_size, data_size) + \
            temp_arr[:remaining_bytes]
        temp_arr = temp_arr[remaining_bytes:]

        # Read the CRC separately
        crc = struct.unpack("<I", temp_arr[:4])[0]
        temp_arr = temp_arr[4:]

        # Validate the CRC
        if crc != zlib.crc32(payload):
            raise Exception("CRC mismatch")

        # Decode the elementes of the payload
        name = payload[8:(name_size+8)].decode(encoding="UTF-16LE").strip("\0")
        guid_bytes = payload[(name_size+8):(name_size+8+16)]
        guid = uuid.UUID(bytes=guid_bytes)
        attributes_bytes = payload[(name_size+8+16):(name_size+8+16+4)]
        attributes = struct.unpack("<I", attributes_bytes)[0]
        data = payload[(name_size+8+16+4):]

        variables.append(UEFIVariable(name, guid, data, attributes))

    # If there are no more entries left, returns
    return variables


def uefi_variables_to_knobs(schema, variables):
    for variable in variables:
        knob = schema.get_knob(variable.name)
        knob.value = knob.format.binary_to_object(variable.data)


def read_csv(schema, csv_path):
    with open(csv_path, 'r') as csv_file:
        reader = csv.reader(csv_file)
        header = next(reader)

        knob_index = 0
        value_index = 0
        try:
            knob_index = header.index('Knob')
        except ValueError:
            raise ParseError("CSV is missing 'Knob' column header")

        try:
            value_index = header.index('Value')
        except ValueError:
            raise ParseError("CSV is missing 'Value' column header")

        for row in reader:
            knob_name = row[knob_index]
            knob_value_string = row[value_index]

            knob = schema.get_knob(knob_name)
            knob.value = knob.format.string_to_object(knob_value_string)


def write_csv(schema, csv_path, subknobs=True):
    with open(csv_path, 'w', newline='') as csv_file:
        writer = csv.writer(csv_file)

        if subknobs:
            writer.writerow(['Knob', 'Value', 'Help'])
            for subknob in schema.subknobs:
                writer.writerow([
                    subknob.name,
                    subknob.format.object_to_string(subknob.default),
                    subknob.help])
        else:
            writer.writerow(['Knob', 'Value', 'Help'])
            for knob in schema.knobs:
                writer.writerow([
                    knob.name,
                    knob.format.object_to_string(knob.default),
                    knob.help])


def write_vlist(schema, vlist_path):
    with open(vlist_path, 'wb') as vlist_file:
        buf = binarize_vlist(schema)
        vlist_file.write(buf)


def usage():
    print("Commands:\n")
    print("  writevl <schema.xml> [<values.csv>] <blob.vl>")
    print("  writecsv <schema.xml> [<blob.vl>] <values.csv>")
    print("")
    print("schema.xml : An XML with the definition of a set of known")
    print("             UEFI variables ('knobs') and types to interpret them")
    print("blob.vl : file is a binary list of UEFI variables in the")
    print("          format used by the EFI 'dmpstore' command")
    print("values.csv : file is a text list of knobs")


def main():
    if len(sys.argv) < 2:
        usage()
        sys.stderr.write('Must provide a command.\n')
        sys.exit(1)
        return

    if sys.argv[1].lower() == "writevl":
        if len(sys.argv) == 4:
            schema_path = sys.argv[2]
            vlist_path = sys.argv[3]

            # Load the schema
            schema = Schema.load(schema_path)

            # Assign all values to their defaults
            for knob in schema.knobs:
                knob.value = knob.default

            # Write the vlist
            write_vlist(schema, vlist_path)
        elif len(sys.argv) == 5:
            schema_path = sys.argv[2]
            values_path = sys.argv[3]
            vlist_path = sys.argv[4]

            # Load the schema
            schema = Schema.load(schema_path)

            # Read values from the CSV
            read_csv(schema, values_path)

            # Write the vlist
            write_vlist(schema, vlist_path)
        else:
            usage()
            sys.stderr.write('Invalid number of arguments.\n')
            sys.exit(1)
            return

    if sys.argv[1].lower() == "writecsv":
        if len(sys.argv) == 4:
            schema_path = sys.argv[2]
            csv_path = sys.argv[3]
            # Load the schema
            schema = Schema.load(schema_path)

            # Assign all values to their defaults
            for knob in schema.knobs:
                knob.value = knob.default

            # Write the vlist
            write_csv(schema, csv_path)
        elif len(sys.argv) == 5:
            schema_path = sys.argv[2]
            vlist_path = sys.argv[3]
            csv_path = sys.argv[4]

            # Load the schema
            schema = Schema.load(schema_path)

            # Read values from the vlist
            read_vlist(schema, vlist_path)

            # Write the CSV
            write_csv(schema, csv_path)
        else:
            usage()
            sys.stderr.write('Invalid number of arguments.\n')
            sys.exit(1)
            return


if __name__ == '__main__':
    sys.exit(main())
