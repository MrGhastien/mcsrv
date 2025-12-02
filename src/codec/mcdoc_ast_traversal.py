from mcdoc_common import *
from mcdoc_ast_nodes import *

from typing import List, Tuple, Optional, Union

from mcdoc_types import TypeKind

def print_elements(elems, delims):
    if not elems or len(elems) == 0:
        return
    print(delims[0], end='')
    for i in elems[:-1]:
        print(f"{i}, ", end='')
    print(f"{elems[-1]}{delims[1]}", end='')

def _traverse_internal(root: ASTNode, indent: int):
    print('')
    print("    " * indent, end='')
    typ = type(root)
    print(typ, end='')
    match root:
        case McdocFile(things=things):
            for t in things:
                _traverse_internal(t, indent + 1)
        case Identifier(name=name):
            print(name)
        case Path() as path:
            print(f" {path}")
        case AttributeNode() as a:
            print(f"Attribute {a.name}", end='')
            if a.value is not None:
                _traverse_internal(a.value, indent + 1)
        case AttributeValueNode() as v:
            if v.name is not None:
                print(v.name)
            if type(v.value) is McdocTypeNode:
                _traverse_internal(v.value, indent + 1)
            else:
                for sub_v in v.value:
                    _traverse_internal(sub_v, indent + 1)
        case UnattrTypeRefNode() as ref:
            print(f" {ref.path}", end='')
        case UnattrSimpleTypeNode() as simple:
            print(f" {simple.kind}", end='')
            if simple.literal is not None:
                print(f"({simple.literal})", end='')
            if simple.value_range is not None:
                print(f" @ {simple.value_range}", end='')
        case UnattrArrayTypeNode() as array:
            print(f"{array.array_element_kind} [] ", end='')
            if array.array_size_range is not None:
                print(array.array_size_range, end='')
        case UnattrListTypeNode() as lst:
            if lst.size_range:
                print(f" @ {lst.size_range}", end='')
            _traverse_internal(lst.elem_type, indent + 1)
        case UnattrCompositeTypeNode() as compound:
            print(f" {compound.kind}", end='')
            for e in compound.elements:
                _traverse_internal(e, indent + 1)
        case UnattrStructTypeNode() as struct:
            if struct.name is not None:
                print(f" {struct.name}", end='')
            for f in struct.fields:
                _traverse_internal(f, indent + 1)
        case StructFieldNode() as field:
            print(' ', end='')
            if type(field.key) is Identifier:
                print(field.key, end='')
            else:
                _traverse_internal(field.key, indent + 1)
            if field.optional:
                print('?', end='')
            for a in field.attributes:
                _traverse_internal(a, indent + 1)
            _traverse_internal(field.type, indent + 1)
        case McdocTypeNode() as type_node:
            if type_node.attributes:
                for a in type_node.attributes:
                    _traverse_internal(a, indent + 1)
            print_elements(type_node.indices, '[]')
            _traverse_internal(type_node.unattr_type, indent + 1)
        case UnattrEnumTypeNode() as enum:
            if enum.name is not None:
                print(f" {enum.name}", end='')
            print(f"({enum.value_kind})")
            for f in enum.fields:
                print(f"{'    ' * (indent + 1)}{f[0]}: {f[1]}")
        case UnattrDispatcherTypeNode() as dispatch_type:
            print(f" {dispatch_type.source}", end='')
            print_elements(dispatch_type.indices, '[]')
        case UseStatement() as use:
            print(f" {use.path}{f" -> {use.alias}" if use.alias is not None else ''}")
        case TypeAliasStatement() as typedef:
            print(f"{typedef.alias}", end='')
            print_elements(typedef.params, '<>')
            _traverse_internal(typedef.target, indent + 1)
        case DispatchStatement() as dispatch:
            print(f" {dispatch.source}", end='')
            print_elements(dispatch.type_params, '<>')
            print(" ->", end='')
            _traverse_internal(dispatch.target, indent + 1)
        case _:
            print(f"Warning: Not traversing {typ}")

def traverse(root: ASTNode):
    _traverse_internal(root, 0)
