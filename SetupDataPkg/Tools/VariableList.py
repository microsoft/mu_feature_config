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
import os
import xmlschema
from xml.dom.minidom import parse, parseString
from enum import Enum


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


class InvalidRangeError(Exception):
    def __init__(self, message):
        super().__init__(message)


# The evaluation context is used to determine how to construct implicit values from types
# When an empty string is evaluated in a "default" tag, it will implicitly be interpreted as
# the default for that type. Similarly, if an empty string is evaluated in a "min" tag, it will
# implicitly be interpreted as the "min" value of the type.
class StringEvaluationContext(Enum):
    DEFAULT = 1
    MIN = 2
    MAX = 3


# Decodes a comma separated list within curly braces in to a list
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
    c_format = False


# A DataFormat defines how a variable element is serialized
# * to/from strings (as in the XML attributes) as well as
# * to/from binary (as in the variable list)
# * to C data types
class DataFormat:
    def __init__(self, c_type):
        self.c_type = c_type
        self.min = None
        self.max = None
        pass


# Represents all data types that have an object representation as a
# Python int
class IntValueFormat(DataFormat):
    def __init__(self, c_type, min, max, pack_format, c_suffix=""):
        super().__init__(c_type)
        self.pack_format = pack_format
        self.c_suffix = c_suffix
        self.default = 0
        self.min = min
        self.max = max

    def string_to_object(self, string_representation, eval_context=StringEvaluationContext.DEFAULT):
        if string_representation == "":
            if eval_context == StringEvaluationContext.DEFAULT:
                return self.default
            elif eval_context == StringEvaluationContext.MIN:
                return self.min
            elif eval_context == StringEvaluationContext.MAX:
                return self.max

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
        if options.c_format:
            return "{}{}".format(object_representation, self.c_suffix)
        else:
            return str(object_representation)

    def object_to_binary(self, object_representation):
        return struct.pack(self.pack_format, object_representation)

    def binary_to_object(self, binary_representation):
        return struct.unpack(self.pack_format, binary_representation)[0]

    def size_in_bytes(self):
        return struct.calcsize(self.pack_format)

    def check_bounds(self, value, min, max):
        if value is not None:
            if value < min:
                raise InvalidRangeError("Value {} below minimum of {}".format(value, min))
        if value is not None:
            if value > max:
                raise InvalidRangeError("Value {} above maximum of {}".format(value, max))


# Represents all data types that have an object representation as a
# Python float
class FloatValueFormat(DataFormat):
    def __init__(
            self,
            c_type,
            pack_format,
            c_suffix=""):
        super().__init__(c_type)
        self.pack_format = pack_format
        self.c_suffix = c_suffix
        self.default = 0.0

    def string_to_object(self, string_representation, eval_context=StringEvaluationContext.DEFAULT):
        if string_representation == "":
            if eval_context == StringEvaluationContext.DEFAULT:
                return self.default
            elif eval_context == StringEvaluationContext.MIN:
                return self.min
            elif eval_context == StringEvaluationContext.MAX:
                return self.max
        return float(string_representation)

    def object_to_string(
            self,
            object_representation,
            options=StringFormatOptions()):
        if options.c_format:
            return "{}{}".format(object_representation, self.c_suffix)
        else:
            return str(object_representation)

    def object_to_binary(self, object_representation):
        return struct.pack(self.pack_format, object_representation)

    def binary_to_object(self, binary_representation):
        return struct.unpack(self.pack_format, binary_representation)[0]

    def size_in_bytes(self):
        return struct.calcsize(self.pack_format)

    def check_bounds(self, value, min, max):
        if value is not None and min is not None:
            if value < min:
                raise InvalidRangeError("Value {} below minimum of {}".format(value, min))
        if value is not None and max is not None:
            if value > max:
                raise InvalidRangeError("Value {} above maximum of {}".format(value, max))


# Represents all data types that have an object representation as a Python bool
class BoolFormat(DataFormat):
    def __init__(self):
        super().__init__(c_type='bool')
        self.default = False

    def string_to_object(self, string_representation, eval_context=StringEvaluationContext.DEFAULT):
        if string_representation == "":
            if eval_context == StringEvaluationContext.DEFAULT:
                return self.default
            elif eval_context == StringEvaluationContext.MIN:
                return self.min
            elif eval_context == StringEvaluationContext.MAX:
                return self.max
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

    def check_bounds(self, value, min, max):
        if min is not None:
            raise ParseError("bool may not have min value of {}".format(min))
        if max is not None:
            raise ParseError("bool may not have max value of {}".format(max))
        pass


builtin_types = {
    'uint8_t'  : (lambda: IntValueFormat(c_type='uint8_t', min=0, max=0xff, pack_format='<B')),  # noqa: E203, E501
    'int8_t'   : (lambda: IntValueFormat(c_type='int8_t', min=-0x80, max=0x7f, pack_format='<b')),  # noqa: E203, E501
    'uint16_t' : (lambda: IntValueFormat(c_type='uint16_t', min=0, max=0xffff, pack_format='<H')),  # noqa: E203, E501
    'int16_t'  : (lambda: IntValueFormat(c_type='int16_t', min=-0x8000, max=0x7fff, pack_format='<h')),  # noqa: E203, E501
    'uint32_t' : (lambda: IntValueFormat(c_type='uint32_t', min=0, max=0xffffffff, pack_format='<I', c_suffix="ul")),  # noqa: E203, E501
    'int32_t'  : (lambda: IntValueFormat(c_type='int32_t', min=-0x80000000, max=0x7fffffff, pack_format='<i', c_suffix="l")),  # noqa: E203, E501
    'uint64_t' : (lambda: IntValueFormat(c_type='uint64_t', min=0, max=0xffffffffffffffff, pack_format='<Q', c_suffix="ull")),  # noqa: E203, E501
    'int64_t'  : (lambda: IntValueFormat(c_type='int64_t', min=-0x8000000000000000, max=0x7fffffffffffffff, pack_format='<q', c_suffix="ll")),  # noqa: E203, E501
    'float'    : (lambda: FloatValueFormat(c_type='float', pack_format='<f', c_suffix="f")),  # noqa: E203, E501
    'double'   : (lambda: FloatValueFormat(c_type='double', pack_format='<d')),  # noqa: E203, E501
    'bool'     : (lambda: BoolFormat()),  # noqa: E203, E501
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
            self.number = int(xml_node.getAttribute("value"), 0)
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
        self.default = None

        super().__init__(c_type=self.name)

        for value in xml_node.getElementsByTagName('Value'):
            enumvalue = EnumValue(self.name, value)
            self.values.append(enumvalue)
            if self.default is None:
                self.default = enumvalue.number

        self.default = self.string_to_object(xml_node.getAttribute("default"))

        if xml_node.getAttribute("min") != "":
            raise ParseError("Enum {} may not have a 'min' attribute".format(self.name))
        if xml_node.getAttribute("max") != "":
            raise ParseError("Enum {} may not have a 'max' attribute".format(self.name))

        pass

    def string_to_object(self, string_representation, eval_context=StringEvaluationContext.DEFAULT):
        if string_representation == "":
            if eval_context == StringEvaluationContext.DEFAULT:
                return self.default
            elif eval_context == StringEvaluationContext.MIN:
                return self.min
            elif eval_context == StringEvaluationContext.MAX:
                return self.max

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
                if options.c_format:
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

    def check_bounds(self, value, min, max):
        if min is not None:
            raise ParseError("Enum {} may not have min value of {}".format(self.name, min))
        if max is not None:
            raise ParseError("Enum {} may not have max value of {}".format(self.name, max))

        if value is not None:
            found = False
            for member_value in self.values:
                if value == member_value.number:
                    found = True
                    break
            if not found:
                raise InvalidRangeError("{} is not a valid value of {}".format(value, self.name))


# Represents a user defined struct
class ArrayFormat(DataFormat):
    def __init__(self, child_format, struct_name, member_name, count):
        self.format = child_format
        self.count = count
        self.struct_name = struct_name
        self.member_name = member_name

        super().__init__(c_type=child_format.c_type)

        self.default = []
        self.min = []
        self.max = []
        for i in range(self.count):
            self.default.append(self.format.default)
            self.min.append(self.format.min)
            self.max.append(self.format.max)
        pass

    def string_to_object(self, string_representation, eval_context=StringEvaluationContext.DEFAULT):
        if string_representation == "":
            if eval_context == StringEvaluationContext.DEFAULT:
                return self.default
            elif eval_context == StringEvaluationContext.MIN:
                return self.min
            elif eval_context == StringEvaluationContext.MAX:
                return self.max

        object_array = []

        # If an array is specified with "{1,2,3}" and the number of elements
        #   matches the expected number for the array, decode the array
        # If an array is specified with only a single element "{4}" treat
        #   that as an array in which all elements are set to that value
        segments = split_braces(string_representation)
        if len(segments) == 1:
            repeated_element = self.format.string_to_object(segments[0], eval_context)
            for element in range(self.count):
                object_array.append(repeated_element)
        elif len(segments) == self.count:
            for element in segments:
                object_array.append(self.format.string_to_object(element, eval_context))
        else:
            raise ParseError(
                "Member '{}' of struct '{}' should have {} elements, "
                "but {} were found"
                .format(
                    self.member_name,
                    self.struct_name,
                    self.count,
                    len(segments)))

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
                (i * element_size):((i + 1) * element_size)]
            values.append(self.format.binary_to_object(element_binary))
        return values

    def size_in_bytes(self):
        return self.format.size_in_bytes() * self.count

    def check_bounds(self, value, min, max):
        for i in range(self.count):
            try:
                self.format.check_bounds(value[i], min[i], max[i])
            except InvalidRangeError as e:
                raise InvalidRangeError("At index {}: {}".format(i, e))


# Represents a member of a user defined struct
# Each member may be of any type, and may also have a count
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

        self.default = self.format.string_to_object(xml_node.getAttribute("default"))
        self.min = self.format.string_to_object(xml_node.getAttribute("min"), StringEvaluationContext.MIN)
        self.max = self.format.string_to_object(xml_node.getAttribute("max"), StringEvaluationContext.MAX)

        self.format.check_bounds(self.min, self.format.min, self.format.max)
        self.format.check_bounds(self.max, self.format.min, self.format.max)
        self.format.check_bounds(self.default, self.min, self.max)

    def string_to_object(self, string_representation, eval_context=StringEvaluationContext.DEFAULT):
        return self.format.string_to_object(string_representation, eval_context)

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

    def check_bounds(self, value, min, max):
        try:
            self.format.check_bounds(value, min, max)
        except InvalidRangeError as e:
            raise InvalidRangeError("Member {} of {}: {}".format(self.name, self.struct_name, e))


# Represents a user defined struct
class StructFormat(DataFormat):
    def __init__(self, schema, xml_node):
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError("Struct name '{}' is invalid".format(
                self.name))
        self.help = xml_node.getAttribute("help")
        self.members = []

        super().__init__(c_type=self.name)

        for member in xml_node.getElementsByTagName('Member'):
            self.members.append(StructMember(schema, self.name, member))

        # Struct definitions themselves should not include default/min/max tags
        # The default/min/max values are interpreted from the struct members

        if xml_node.getAttribute("default") != "":
            raise ParseError("Struct {} may not have a 'default' attribute".format(self.name))
        if xml_node.getAttribute("min") != "":
            raise ParseError("Struct {} may not have a 'min' attribute".format(self.name))
        if xml_node.getAttribute("max") != "":
            raise ParseError("Struct {} may not have a 'max' attribute".format(self.name))

        # Construct the default value from the members
        self.default = self.string_to_object('')

        self.min = OrderedDict()
        self.max = OrderedDict()

        for member in self.members:
            self.min[member.name] = member.min
            self.max[member.name] = member.max

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

    def string_to_object(self, string_representation, eval_context=StringEvaluationContext.DEFAULT):
        obj = OrderedDict()

        segments = split_braces(string_representation)

        # Get the defaults from the member values
        if len(segments) == 0 or (len(segments) == 1 and segments[0] == ""):
            for member in self.members:
                if eval_context == StringEvaluationContext.DEFAULT:
                    obj[member.name] = member.default
                elif eval_context == StringEvaluationContext.MIN:
                    obj[member.name] = member.min
                elif eval_context == StringEvaluationContext.MAX:
                    obj[member.name] = member.max
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
            obj[member.name] = member.string_to_object(value, eval_context)
        return obj

    def object_to_string(
            self,
            object_representation,
            options=StringFormatOptions()):
        member_strings = []

        for member in self.members:
            value = object_representation[member.name]
            if options.c_format:
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
                position:(position + member.size_in_bytes())]
            member_value = member.binary_to_object(member_binary)
            obj[member.name] = member_value
            position += member.size_in_bytes()
        return obj

    def size_in_bytes(self):
        total_size = 0
        for member in self.members:
            total_size += member.size_in_bytes()
        return total_size

    def check_bounds(self, value, min, max):
        for member in self.members:
            member_value = value[member.name]
            member_min = min[member.name]
            member_max = max[member.name]
            member.check_bounds(member_value, member_min, member_max)


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

        self._value = None

        # Use the format to decode the default value from its
        # string representation
        try:
            self._default = self.format.string_to_object(xml_node.getAttribute("default"))
        except ParseError as e:
            # Capture the ParseError and create a more specific error
            # message
            raise ParseError(
                "Unable to parse default value of '{}': {}".format(
                    self.name,
                    e))

        try:
            self._min = self.format.string_to_object(xml_node.getAttribute("min"), StringEvaluationContext.MIN)
        except ParseError as e:
            raise ParseError(
                "Unable to parse the min value of '{}': {}".format(
                    self.name, e
                )
            )

        self.format.check_bounds(self._min, self.format.min, self.format.max)

        try:
            self._max = self.format.string_to_object(xml_node.getAttribute("max"), StringEvaluationContext.MAX)
        except ParseError as e:
            raise ParseError(
                "Unable to parse the max value of '{}': {}".format(
                    self.name, e
                )
            )

        self.format.check_bounds(self._max, self.format.min, self.format.max)

        self.format.check_bounds(self._default, self._min, self._max)

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

    @property
    def min(self):
        return copy.deepcopy(self._min)

    @property
    def max(self):
        return copy.deepcopy(self._max)

    @property
    def value(self):
        return copy.deepcopy(self._value)

    @value.setter
    def value(self, new_value):
        if new_value is not None:
            self.format.check_bounds(new_value, self._min, self._max)
        self._value = new_value

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

            # The full object is the entire object represented by the knob
            # the child object will be updated to point to sub-objects within the full object
            # until the final value can be updated
            full_object = child_object

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

            # The last iteration is in reference to a value, not an object, so we
            # will use the last child_object pointer to update the value
            (name, index) = self._decode_subpath(final_element)
            if index is None:
                child_object[name] = value
            else:
                child_object[name][index] = value

            # Update the knob to the value of the full_object, which has been modified by virtue of
            # updating the child object (which was a pointer to an internal structure of the full object)
            self.value = full_object

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
        self.knob._set_child_value(self.name, value)
        pass

    @property
    def default(self):
        return self.knob._get_child_value(self.name, self.knob.default)

    @property
    def min(self):
        return self.knob._get_child_value(self.name, self.knob.min)

    @property
    def max(self):
        return self.knob._get_child_value(self.name, self.knob.max)


class Schema:
    def __init__(self, dom, origin_path=""):
        self.enums = []
        self.structs = []
        self.knobs = []
        self.path = origin_path

        for section in dom.getElementsByTagName('Enums'):
            for enum in section.getElementsByTagName('Enum'):
                self.enums.append(EnumFormat(enum))

        for section in dom.getElementsByTagName('Structs'):
            for struct_node in section.getElementsByTagName('Struct'):
                self.structs.append(StructFormat(self, struct_node))

        for section in dom.getElementsByTagName('Knobs'):
            namespace = section.getAttribute('namespace')
            for knob in section.getElementsByTagName('Knob'):
                self.knobs.append(Knob(self, knob, namespace))

        self.subknobs = []
        for knob in self.knobs:
            self.subknobs += knob.subknobs

        pass

    def find_data_file(filename):
        if getattr(sys, "frozen", False):
            # The application is frozen
            datadir = os.path.dirname(sys.executable)
        else:
            # The application is not frozen
            # Change this bit to match where you store your data files:
            datadir = os.path.dirname(os.path.abspath(__file__))
        return os.path.join(datadir, filename)

    # Load a schema given a path to a schema xml file
    def load(path):

        # Get the XML schema from the current path
        # Per instructions from cx_freeze: https://cx-freeze.readthedocs.io/en/latest/faq.html#using-data-files
        xsd = xmlschema.XMLSchema(Schema.find_data_file("configschema.xsd"))

        # raises exception if validation fails
        xsd.validate(path)

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
    #   CRC32(int32 checksum of all bytes from NameSize through Data)

    # Null terminated UTF-16LE encoded name
    name = (variable.name + "\0").encode("utf-16le")
    data = variable.data
    guid = variable.guid.bytes_le
    attributes = variable.attributes

    payload = struct.pack("<ii", len(name), len(data)) + \
        name + \
        guid + \
        struct.pack("<I", attributes) + \
        data

    crc = zlib.crc32(payload)

    return payload + struct.pack("<I", crc)


def get_delta_vlist(schema):
    name_list = []
    var_list = []
    for knob in schema.knobs:
        if knob.default == knob.value:
            # knob value didn't change
            continue
        value_bytes = knob.format.object_to_binary(knob.value)

        variable = UEFIVariable(knob.name, knob.namespace, value_bytes)
        var_list.append(create_vlist_buffer(variable))
        name_list.append(knob.name)

    return name_list, var_list


# Create a byte array for all the knobs in this schema
def vlist_to_binary(schema):

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

        # Decode the elements of the payload
        name = payload[8:(name_size + 8)].decode(encoding="UTF-16LE").strip("\0")
        guid_bytes = payload[(name_size + 8):(name_size + 8 + 16)]
        guid = uuid.UUID(bytes_le=guid_bytes)
        attributes_bytes = payload[(name_size + 8 + 16):(name_size + 8 + 16 + 4)]
        attributes = struct.unpack("<I", attributes_bytes)[0]
        data = payload[(name_size + 8 + 16 + 4):]

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


def write_csv(schema, csv_path, full, subknobs=True):
    with open(csv_path, 'w', newline='') as csv_file:
        writer = csv.writer(csv_file)

        # get_delta_vlist is a tuple of name_list, var_list
        name_list = get_delta_vlist(schema)[0]

        if subknobs:
            writer.writerow(['Knob', 'Value', 'Binary', 'Comment'])
            for subknob in schema.subknobs:
                if full or subknob.name in name_list:
                    binary = subknob.format.object_to_binary(subknob.value)
                    string_binary = " ".join(map("%2.2x".__mod__, binary))
                    writer.writerow([
                        subknob.name,
                        subknob.format.object_to_string(subknob.value),
                        string_binary,
                        subknob.help])
        else:
            writer.writerow(['Knob', 'Value', 'Binary', 'Comment'])
            for knob in schema.knobs:
                if full or knob.name in name_list:
                    binary = knob.format.object_to_binary(knob.value)
                    string_binary = " ".join(map("%2.2x".__mod__, binary))
                    writer.writerow([
                        knob.name,
                        knob.format.object_to_string(knob.value),
                        string_binary,
                        knob.help])


def write_vlist(schema, vlist_path):
    with open(vlist_path, 'wb') as vlist_file:
        buf = vlist_to_binary(schema)
        vlist_file.write(buf)


def usage():
    print("Commands:\n")
    print("  write_vl <schema.xml> [<values.csv>] <blob.vl>")
    print("  write_csv <schema.xml> [<blob.vl>] <values.csv>")
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

    if sys.argv[1].lower() == "write_vl":
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

    if sys.argv[1].lower() == "write_csv":
        if len(sys.argv) == 4:
            schema_path = sys.argv[2]
            csv_path = sys.argv[3]
            # Load the schema
            schema = Schema.load(schema_path)

            # Assign all values to their defaults
            for knob in schema.knobs:
                knob.value = knob.default

            # Write the full vlist with complete knobs
            write_csv(schema, csv_path, True, False)
        elif len(sys.argv) == 5:
            schema_path = sys.argv[2]
            vlist_path = sys.argv[3]
            csv_path = sys.argv[4]

            # Load the schema
            schema = Schema.load(schema_path)

            # Read values from the vlist
            read_vlist(schema, vlist_path)

            # Write the full vlist CSV with complete knobs
            write_csv(schema, csv_path, True, False)
        else:
            usage()
            sys.stderr.write('Invalid number of arguments.\n')
            sys.exit(1)
            return


if __name__ == '__main__':
    sys.exit(main())
