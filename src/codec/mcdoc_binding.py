from mcdoc_types import *
from mcdoc_ast_nodes import *
import pathlib

base_path: Path = None

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

class McdocFile():
    types: List[McdocType]
    uses: List[Path]
    path: Path
    fs_path: pathlib.Path

    def __init__(self):
        self.types = []
        self.uses = []

    def register_type(self, typ: McdocType):
        if typ is None or type(typ) is TypeRef:
            return
        name = typ.get_name()
        if self.find_type(name) is not None:
            raise SyntaxError(f"Duplicate types {name} (class is {type(typ)}).")
        self.types.append(typ)

    def find_type(self, name: str) -> McdocType:
        if name is None:
            return None
        for t in self.types:
            if t.get_name() == name:
                return t
        return None

    def __repr__(self):
        return f"{self.types}"

files : List[McdocFile] = []

def kind_to_type(kind: TypeKind) -> BuiltinType:
    for t in builtin_types:
        if t.type is kind:
            return t
    return None
        

def find_type_global(name: str, file: McdocFile) -> McdocType:
    t = file.find_type(name)
    if t is not None:
        return t
    

class SemanticError(Exception):
    def __init__(self, message: str, node=None):
        super().__init__(message)

def bind_attribute(attr_node: AttributeNode) -> Attribute:
    pass

def bind_union(node: UnattrCompositeTypeNode, attrs: List[Attribute], file: McdocFile) -> UnionType:
    return UnionType(attributes=attrs, elements=[bind_type(e, file) for e in node.elements])

def bind_tuple(node: UnattrCompositeTypeNode, attrs: List[Attribute], file: McdocFile) -> TupleType:
    return TupleType(attributes=attrs, elements=[bind_type(e, file) for e in node.elements])

def bind_ref(node: UnattrTypeRefNode, attrs: List[Attribute], file: McdocFile) -> McdocType:
    t = file.find_type(node.path.segments[-1])
    if t is None:
        return TypeRef(target=node.path)
    return t

def bind_list(node: UnattrListTypeNode, attrs: List[Attribute], file: McdocFile) -> ListType:
    return ListType(attributes=attrs, elem_type=bind_type(node.elem_type, file), size_range=node.size_range)

def bind_simple(node: UnattrSimpleTypeNode, attrs: List[Attribute]) -> Union[BuiltinType, LiteralType]:
    if node.literal:
        return LiteralType(attributes=attrs, type=node.kind, value=node.literal, value_range=node.value_range)
    return kind_to_type(node.kind)
        
def bind_type(type_node: McdocTypeNode, file: McdocFile) -> McdocType:
    attrs = []
    for a_node in type_node.attributes:
        attrs.append(a_node)

    match type_node.unattr_type:
        case UnattrStructTypeNode() as struct_node:
            return bind_struct(struct_node, attrs)
        case UnattrEnumTypeNode() as enum_node:
            return bind_enum(enum_node, attrs)
        case UnattrListTypeNode() as list_node:
            return bind_list(list_node, attrs, file)
        case UnattrCompositeTypeNode() as composite:
            if composite.kind is TypeKind.UNION:
                return bind_union(composite, attrs, file)
            else:
                return bind_tuple(composite, attrs, file)
        # case UnattrArrayTypeNode() as array:
        #     return bind_array(array, attrs)
        case UnattrTypeRefNode() as ref:
            return bind_ref(ref, attrs, file)
        case UnattrSimpleTypeNode() as simple:
            return bind_simple(simple, attrs)
        case UnattrArrayTypeNode() as arr:
            pass
        case UnattrDispatcherTypeNode() as dispatcher:
            pass
        case _:
            raise SyntaxError(f"Not handling type {type(type_node.unattr_type)}.")

def bind_struct_field(field_node: StructFieldNode) -> StructField:
    if type(field_node) is McdocTypeNode:
        return StructField(key_type=field_node.key, typ=field_node.type)

    if field_node.spread:
        return StructField(typ=field_node.type, spread=True, attributes=[])

    return StructField(name=field_node.key.name if type(field_node.key) is Identifier else None, typ=field_node.type, attributes=[])

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

def bind_dispatch(dispatch_node: DispatchStatement, attrs: List[Attribute], file: McdocFile) -> McdocType:
    return bind_type(dispatch_node.target, file)

def bind_alias(alias_node: TypeAliasStatement, attrs: List[Attribute], file: McdocFile) -> McdocType:
    res =  bind_type(alias_node.target, file)

    match res:
        case StructType() as struct:
            file.register_type(struct)
        case EnumType() as enumm:
            file.register_type(enumm)
    file.register_type(TypeAlias(attributes=attrs, source=res, name=alias_node.alias))

def bind_file(file_node: McdocFileNode) -> McdocFile:
    file: McdocFile = McdocFile()
    for x in file_node.things:
        match x:
            case McdocTypeNode() as type_node:
                bind_type(type_node)
            case UnattrEnumTypeNode() as enum_node:
                x = bind_enum(enum_node, [])
                file.register_type(x)
            case UnattrStructTypeNode() as struct_node:
                x = bind_struct(struct_node, [])
                file.register_type(x)
            case DispatchStatement() as dispatch_node:
                x = bind_dispatch(dispatch_node, [], file)
                file.register_type(x)
            case TypeAliasStatement() as alias_node:
                x = bind_alias(alias_node, [], file)
            case UseStatement() as use:
                file.uses.append(use.path)
            case _:
                print(f"[BIND] Warning: not traversing {type(x)}")
    return file

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
            print(f"[RESOLV] Warning: Not traversing {type(typ)}")

def resolve_refs():
    for t in types:
        resolve_ref(t)
