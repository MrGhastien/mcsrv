from mcdoc_types import *
types = []

class SemanticError(Exception):
    def __init__(self, message: str, node=None):
        super().__init__(message)

def traverse_struct_field(field_node: StructFieldNode) -> StructField:
    if type(field_node) is McdocTypeNode:
        return StructField(key_type=field_node.key, typ=field_node.type)

    return StructField(name=field_node.key.name, typ=field_node.type)

def _traverse_internal(root: ASTNode, indent: int):
    match root:
        case McdocFile(things=things):
            for t in things:
                _traverse_internal(t, indent + 1)
        case Identifier(name=name):
            pass
        case Path() as path:
            pass
        case Attribute() as a:
            if a.value is not None:
                _traverse_internal(a.value, indent + 1)
        case AttributeValue() as v:
            if type(v.value) is McdocTypeNode:
                _traverse_internal(v.value, indent + 1)
            else:
                for sub_v in v.value:
                    _traverse_internal(sub_v, indent + 1)
        case UnattrTypeRefNode() as ref:
            pass
        case UnattrSimpleTypeNode() as simple:
            pass
        case UnattrArrayTypeNode() as array:
            pass
        case UnattrListTypeNode() as lst:
            _traverse_internal(lst.elem_type, indent + 1)
        case UnattrCompositeTypeNode() as compound:
            for e in compound.elements:
                _traverse_internal(e, indent + 1)
        case UnattrStructTypeNode() as struct:
            for f in struct.fields:
                _traverse_internal(f, indent + 1)
        case StructFieldNode() as field:
            if type(field.key) is not Identifier:
                _traverse_internal(field.key, indent + 1)
            for a in field.attributes:
                _traverse_internal(a, indent + 1)
            _traverse_internal(field.type, indent + 1)
        case McdocTypeNode() as type_node:
            if type_node.attributes:
                for a in type_node.attributes:
                    _traverse_internal(a, indent + 1)
            #print_elements(type_node.indices, '[]')
            _traverse_internal(type_node.unattr_type, indent + 1)
        case UnattrEnumTypeNode() as enum:
            pass
        case UnattrDispatcherTypeNode() as dispatch_type:
            #print_elements(dispatch_type.indices, '[]')
            pass
        case UseStatement() as use:
            pass
        case TypeAliasStatement() as typedef:
            #print_elements(typedef.params, '<>')
            _traverse_internal(typedef.target, indent + 1)
        case DispatchStatement() as dispatch:
            #print_elements(dispatch.type_params, '<>')
            _traverse_internal(dispatch.target, indent + 1)
        case _:
            print(f"Warning: Not traversing {typ}")
