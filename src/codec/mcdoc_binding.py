from mcdoc_types import *
from mcdoc_ast_nodes import *
types : List[McdocType] = []
to_find = []

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

def bind_attribute(attr_node: AttributeNode) -> Attribute:
    pass

def bind_union(node: UnattrCompositeTypeNode, attrs: List[Attribute]) -> UnionType:
    return UnionType(attributes=attrs, elements=[bind_type(e) for e in node.elements ])

def bind_tuple(node: UnattrCompositeTypeNode, attrs: List[Attribute]) -> TupleType:
    return TupleType(attributes=attrs, elements=[bind_type(e) for e in node.elements ])

def bind_ref(node: UnattrTypeRefNode, attrs: List[Attribute]) -> McdocType:
    for t in types:
        print(t.name)
        if t.name == node.path.segments[-1]:
            return t
    return TypeRef(target=node.path)

def bind_list(node: UnattrListTypeNode, attrs: List[Attribute]) -> ListType:
    return ListType(attributes=attrs, elem_type=bind_type(node.elem_type), size_range=node.size_range)

def bind_simple(node: UnattrSimpleTypeNode, attrs: List[Attribute]) -> Union[BuiltinType, LiteralType]:
    if node.literal:
        return LiteralType(attributes=attrs, type=node.kind, value=node.literal, value_range=node.value_range)
    return kind_to_type(node.kind)
        
def bind_type(type_node: McdocTypeNode) -> McdocType:
    attrs = []
    for a_node in type_node.attributes:
        attrs.append(a_node)

    match type_node.unattr_type:
        case UnattrStructTypeNode() as struct_node:
            return bind_struct(struct_node, attrs)
        case UnattrEnumTypeNode() as enum_node:
            return bind_enum(enum_node, attrs)
        case UnattrListTypeNode() as list_node:
            return bind_list(list_node, attrs)
        case UnattrCompositeTypeNode() as composite:
            if composite.kind is TypeKind.UNION:
                return bind_union(composite, attrs)
            else:
                return bind_tuple(composite, attrs)
        case UnattrArrayTypeNode() as array:
            return bind_array(array, attrs)
        case UnattrTypeRefNode() as ref:
            return bind_ref(ref, attrs)
        case UnattrSimpleTypeNode() as simple:
            return bind_simple(simple, attrs)
        case _:
            print(f"Not handling type {type(type_node)}.")

def bind_struct_field(field_node: StructFieldNode) -> StructField:
    if type(field_node) is McdocTypeNode:
        return StructField(key_type=field_node.key, typ=field_node.type)

    if field_node.spread:
        print("TODO: Handle spreading fields.")
        return None
    return StructField(name=field_node.key.name, typ=field_node.type, attributes=[])

def bind_struct(struct_node: UnattrStructTypeNode, attrs: List[Attribute]) -> StructType:
    fields = []
    for f_node in struct_node.fields:
        f = bind_struct_field(f_node)
        if f:
            fields.append(f)
    name: Optional[str] = None
    if struct_node.name is not None:
        name = struct_node.name.name

    return StructType(attributes=attrs, fields=fields, name=name)

def bind_enum_field(field_node: Tuple[Identifier, Union[int, float, str]], expected_type: BuiltinType) -> EnumField:
    return EnumField(name=field_node[0].name, value=field_node[1])

def bind_enum(enum_node: UnattrEnumTypeNode, attrs: List[Attribute]) -> EnumType:
    typ = kind_to_type(enum_node.value_kind)
    fields = []
    for f_node in enum_node.fields:
        fields.append(bind_enum_field(f_node, typ))

    return EnumType(attributes=attrs, type=typ, fields=fields, name=enum_node.name.name)

def bind_dispatch(dispatch_node: DispatchStatement, attrs: List[Attribute]) -> McdocType:
    return bind_type(dispatch_node.target)

def bind_alias(alias_node: TypeAliasStatement, attrs: List[Attribute]) -> McdocType:
    res =  bind_type(alias_node.target)

    match res:
        case StructType() as struct:
            types.append(struct)
        case EnumType() as enumm:
            types.append(enumm)
    types.append(TypeAlias(attributes=attrs, source=res, name=alias_node.alias))

def bind_file(file_node: McdocFile):
    for x in file_node.things:
        match x:
            case McdocTypeNode() as type_node:
                bind_type(type_node)
            case UnattrEnumTypeNode() as enum_node:
                x = bind_enum(enum_node, [])
                types.append(x)
            case UnattrStructTypeNode() as struct_node:
                x = bind_struct(struct_node, [])
                types.append(x)
            case DispatchStatement() as dispatch_node:
                x = bind_dispatch(dispatch_node, [])
            case TypeAliasStatement() as alias_node:
                x = bind_alias(alias_node, [])
            case _:
                print(f"[BIND] Warning: not traversing {type(x)}")

def search_type_with_name(name: str) -> McdocType:
    for t in types:
        if t.get_name() == name:
            return t
    raise SyntaxError(f"Unknown type {name} ().")

def resolve_type_ref(type: McdocType) -> McdocType:
    match type:
        case TypeRef():
            return search_type_with_name(type.get_name())
        case _:
            resolve_ref(type)
            return type

def resolve_ref(typ: McdocType):
    match typ:
        case StructType(fields=fields):
            for f in fields:
                resolve_ref(f)
        case StructField() as f:
            f.key_type = resolve_type_ref(f.key_type)
            f.typ = resolve_type_ref(f.typ)
        case UnionType() as union:
            for i in range(len(union.elements)):
                union.elements[i] = resolve_type_ref(union.elements[i])
        case TupleType() as tuple:
            for i in range(len(tuple.elements)):
                tuple.elements[i] = resolve_type_ref(tuple.elements[i])
        case ListType() as lst:
            lst.elem_types = resolve_type_ref(lst.elem_type)
        case TypeAlias() as alias:
            alias.source = resolve_type_ref(alias.source)
        case _:
            print(f"Warning: Not traversing {type(typ)}")

def resolve_refs():
    for t in types:
        resolve_ref(t)

def bind_bind(root: ASTNode):
    bind_file(root)
    resolve_refs()
    print(types)
