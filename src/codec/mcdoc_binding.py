from mcdoc_types import *
from mcdoc_ast_nodes import *
types = []

builtin_types = [
    BuiltinType(TypeKind.STRING),
    BuiltinType(TypeKind.BYTE),
    BuiltinType(TypeKind.SHORT),
    BuiltinType(TypeKind.INT),
    BuiltinType(TypeKind.LONG),
    BuiltinType(TypeKind.BOOLEAN),
    BuiltinType(TypeKind.FLOAT),
    BuiltinType(TypeKind.DOUBLE),
    BuiltinType(TypeKind.ANY),
]

def kind_to_type(kind: TypeKind) -> BuiltinType:
    for t in builtin_types:
        if t.type is kind:
            return t
    return None

class SemanticError(Exception):
    def __init__(self, message: str, node=None):
        super().__init__(message)

def traverse_attribute(attr_node: AttributeNode) -> Attribute:
    pass
        
def traverse_type(type_node: McdocTypeNode) -> McdocType:
    attrs = []
    for a_node in type_node.attributes:
        attrs.append(a_node)

    match type_node.unattr_type:
        case UnattrStructTypeNode() as struct_node:
            res = travserse_struct(struct, attrs)
            if res.name is not None:
                types.append(res)
            return res
        case UnattEnumTypeNode() as enum_node:
            res = traverse_enum(struct, attrs)
            if res.name is not None:
                types.append(res)
            return res
        case UnattrListTypeNode() as list_node:
            return traverse_list(list_node, attrs)
        case UnattrCompositeTypeNode() as composite:
            if composite.kind is TypeKind.UNION:
                return traverse_union(composite, attrs)
            else:
                return traverse_tuple(composite, attrs)
        case UnattrArrayTypeNode() as array:
            return traverse_array(array, attrs)
        case UnattrTypeRefNode() as ref:
            return traverse_ref(ref, attrs)
        case UnattrSimpleTypeNode() as simple:
            return traverse_simple(simple, attrs)

def traverse_struct_field(field_node: StructFieldNode) -> StructField:
    if type(field_node) is McdocTypeNode:
        return StructField(key_type=field_node.key, typ=field_node.type)

    return StructField(name=field_node.key.name, typ=field_node.type, attributes=[])

def traverse_struct(struct_node: UnattrStructTypeNode, attrs: List[Attribute]) -> StructType:
    fields = []
    for f_node in struct_node.fields:
        fields.append(traverse_struct_field(f_node))
    name: Optional[str] = None
    if struct_node.name is not None:
        name = struct_node.name.name

    return StructType(attributes=attrs, fields=fields, name=name)

def traverse_enum_field(field_node: Tuple[Identifier, Union[int, float, str]], expected_type: BuiltinType) -> EnumField:
    return EnumField(name=field_node[0].name, value=field_node[1])

def traverse_enum(enum_node: UnattrEnumTypeNode, attrs: List[Attribute]) -> EnumType:
    typ = kind_to_type(enum_node.value_kind)
    fields = []
    for f_node in enum_node.fields:
        fields.append(traverse_enum_field(f_node, typ))

    return EnumType(attributes=attrs, type=typ, fields=fields, name=enum_node.name.name)

def traverse_file(file_node: McdocFile):
    for x in file_node.things:
        match x:
            case McdocTypeNode() as type_node:
                traverse_type(type_node)
            case UnattrEnumTypeNode() as enum_node:
                x = traverse_enum(enum_node, [])
                types.append(x)
            case UnattrStructTypeNode() as struct_node:
                x = traverse_struct(struct_node, [])
                types.append(x)
            case _:
                print(f"[BIND] Warning: not traversing {type(x)}")

def traverse_bind(root: ASTNode):
    traverse_file(root)
    print(types)
