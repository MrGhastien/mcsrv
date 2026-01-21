from mcdoc_types import *
from mcdoc_ast_nodes import *
from pathlib import Path

from typing import List, Dict

base_path: McdocPath = None

builtin_types = [
    BuiltinType(TypeKind.STRING),
    BuiltinType(TypeKind.BYTE, 'i8'),
    BuiltinType(TypeKind.SHORT, 'i16'),
    BuiltinType(TypeKind.INT, 'i32'),
    BuiltinType(TypeKind.LONG, 'i64'),
    BuiltinType(TypeKind.BOOLEAN, 'bool'),
    BuiltinType(TypeKind.FLOAT, 'f32'),
    BuiltinType(TypeKind.DOUBLE, 'f64'),
    BuiltinType(TypeKind.ANY, 'void*'),
]

class McdocFile():
    types: List[McdocType]
    generic_instances: List[GenericTypeInstance]
    uses: List[McdocPath]
    dispatches: Dict[ResourceID, Dict[str, McdocType]]
    fs_path: Path
    is_module: bool

    def __init__(self, fs_path: Path):
        self.types = []
        self.generic_instances = []
        self.uses = []
        self.dispatches = {}
        self.fs_path = fs_path
        self.is_module = fs_path.stem == 'mod'

    def register_type(self, typ: McdocType):
        if typ is None or type(typ) is TypeRef or type(typ) is GenericTypeInstance or type(typ) is BuiltinType or type(typ) is LiteralType:
            return
        name = typ.get_name()
        if name is None:
            return
        if self.find_type(name) is not None:
            raise SyntaxError(f"Duplicate types {name} (class is {type(typ)}).")
        self.types.append(typ)

    def register_generic_instance(self, typ: GenericTypeInstance):
        name = typ.get_name()
        assert name is not None
        existing_normal = self.find_type(name)
        if existing_normal is not None:
            raise SyntaxError(f"Generic instantiation name collision: {name} <-> {existing_normal}.")
        if typ not in self.generic_instances:
            for gi in self.generic_instances:
                if gi.get_name() == name and gi.source != typ.source:
                    raise SyntaxError(f"Duplicate generic instance names {name}.")
        self.generic_instances.append(typ)


    def find_type(self, name: str) -> McdocType:
        if name is None:
            return None
        for t in self.types:
            if t.get_name() == name:
                return t
        return None

    def register_dispatcher(self, resid: ResourceID, index: str, typ: McdocType):
        dis = self.dispatches.get(resid)
        if dis is None:
            self.dispatches[resid] = { index: typ }
        else:
            self.dispatches[resid][index] = typ

    def __repr__(self):
        return f"{self.types}"

files : List[McdocFile] = []

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
    return UnionType(attributes=attrs, elements=[bind_type(e) for e in node.elements])

def bind_tuple(node: UnattrCompositeTypeNode, attrs: List[Attribute]) -> TupleType:
    return TupleType(attributes=attrs, elements=[bind_type(e) for e in node.elements])

def bind_ref(node: UnattrTypeRefNode, attrs: List[Attribute]) -> McdocType:
    return TypeRef(target=node.path)

def bind_list(node: UnattrListTypeNode, attrs: List[Attribute]) -> ListType:
    return ListType(attributes=attrs, elem_type=bind_type(node.elem_type), size_range=node.size_range)

def bind_simple(node: UnattrSimpleTypeNode, attrs: List[Attribute]) -> Union[BuiltinType, LiteralType]:
    builtin = kind_to_type(node.kind)
    if node.literal is not None:
        return LiteralType(attributes=attrs, parent=builtin, value=node.literal, value_range=node.value_range)
    return builtin
        
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
        case UnattrTypeRefNode() as ref:
            if type_node.args:
                new_args = []
                for a in type_node.args:
                    #[bind_type(n) for n in type_node.args]
                    t = bind_type(a)
                    assert t is not None
                    new_args.append(t)
                source_alias = bind_ref(ref, attrs)
                return GenericTypeInstance(attributes=attrs, source=source_alias, args=new_args)
            return bind_ref(ref, attrs)
        case UnattrSimpleTypeNode() as simple:
            return bind_simple(simple, attrs)
        case UnattrArrayTypeNode() as arr:
            return ArrayType(attributes=attrs, elem_type=kind_to_type(arr.array_element_kind), size_range=arr.array_size_range)
        case UnattrDispatcherTypeNode() as dispatcher:
            return DispatcherType(attributes=attrs, source=dispatcher.source, indices=dispatcher.indices)
        case _:
            raise SyntaxError(f"Not handling type {type(type_node.unattr_type)}.")

def bind_struct_field(field_node: StructFieldNode) -> StructField:
    if type(field_node) is McdocTypeNode:
        return StructField(key_type=field_node.key, typ=field_node.type, optional=field_node.optional)

    typ = bind_type(field_node.type)
    if field_node.spread:
        return StructField(typ=typ, spread=True, attributes=[])

    #print(f"{str(field_node.key)} -> {field_node.type.unattr_type.kind}")
    return StructField(name=field_node.key.name if type(field_node.key) is Identifier else None, typ=typ, attributes=[], optional=field_node.optional)

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

def bind_alias(alias_node: TypeAliasStatement, attrs: List[Attribute]) -> Tuple[McdocType, McdocType]:
    res =  bind_type(alias_node.target)
    return (TypeAlias(attributes=attrs, source=res, name=alias_node.alias, params=[ParamType(str(i)) for i in alias_node.params]), res)

def bind_file(file_node: McdocFileNode, path: Path) -> McdocFile:
    file: McdocFile = McdocFile(path)
    for x in file_node.things:
        match x:
            case UnattrEnumTypeNode() as enum_node:
                x = bind_enum(enum_node, [])
                file.register_type(x)
            case UnattrStructTypeNode() as struct_node:
                x = bind_struct(struct_node, [])
                file.register_type(x)
            case DispatchStatement() as dispatch_node:
                x = bind_dispatch(dispatch_node, [])
                file.register_type(x)
            case TypeAliasStatement() as alias_node:
                alias,y = bind_alias(alias_node, [])
                file.register_type(alias)
                file.register_type(y)
            case UseStatement() as use:
                file.uses.append(use.path)
            case _:
                print(f"[BIND] Warning: not traversing {type(x)}")
    return file
