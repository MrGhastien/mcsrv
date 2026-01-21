from mcdoc_common import TokenType, ResourceID
from enum import Enum, auto
from abc import ABC
from dataclasses import dataclass, field

from typing import List, Optional, Union, Tuple

class TypeKind(Enum):
    ANY = 0
    BOOLEAN = auto()
    BYTE = auto()
    SHORT = auto()
    INT = auto()
    LONG = auto()
    FLOAT = auto()
    DOUBLE = auto()
    STRING = auto()
    REF = auto()
    DISPATCHER = auto()
    LIST = auto()
    ARRAY = auto()
    STRUCT = auto()
    ENUM = auto()
    TUPLE = auto()
    UNION = auto()

    def from_token_type(tok_type: TokenType):
        """Infers a type kind from a token type.

        The token should be the first token of a type.
        """
        return _tokentype_to_typekind_table.get(tok_type)

# This table is used to guess the kind of type to parse from just the first token.
# This is also used to 
_tokentype_to_typekind_table = {
    TokenType.KW_ANY: TypeKind.ANY,
    TokenType.KW_BYTE: TypeKind.BYTE,
    TokenType.KW_SHORT: TypeKind.SHORT,
    TokenType.KW_INT: TypeKind.INT,
    TokenType.KW_LONG: TypeKind.LONG,
    TokenType.KW_FLOAT: TypeKind.FLOAT,
    TokenType.KW_DOUBLE: TypeKind.DOUBLE,
    TokenType.KW_STRING: TypeKind.STRING,
    TokenType.KW_FALSE: TypeKind.BOOLEAN,
    TokenType.KW_TRUE: TypeKind.BOOLEAN,
    TokenType.KW_BOOLEAN: TypeKind.BOOLEAN,
    TokenType.KW_ENUM: TypeKind.ENUM,
    TokenType.KW_STRUCT: TypeKind.STRUCT,
    TokenType.STRING: TypeKind.STRING,
    TokenType.INTEGER: TypeKind.INT,
    TokenType.FLOAT: TypeKind.FLOAT,
    TokenType.LPAREN: TypeKind.UNION,
    # TokenType.KW_FALLBACK:
    # TokenType.KW_NONE:
    # TokenType.KW_UNKNOWN:
    # TokenType.KW_KEY:
    # TokenType.KW_PARENT:
    # TokenType.KW_TYPE:
    # TokenType.KW_USE:
    # TokenType.KW_AS:
    # TokenType.KW_INJECT:
    # TokenType.KW_DISPATCH:
    # TokenType.KW_TO:
    # TokenType.KW_SUPER
}

@dataclass
class Range:
    floating: bool
    start: Optional[Union[int, float]] = None
    end: Optional[Union[int, float]] = None
    start_exclusive: bool = False
    end_exclusive: bool = False

    def __str__(self) -> str:
        return f"{self.start if self.start else ''}{'<' if self.start_exclusive else ''}..{'<' if self.end_exclusive else ''}{self.end if self.end else ''}"


@dataclass
class TypeIndex:
    key: List
    dynamic: bool = False

    def __str__(self) -> str:
        if self.dynamic:
            return str(self.key)
        else:
            return str(self.key[0])

    def __repr__(self):
        return str(self)

@dataclass(kw_only=True, repr=False)
class McdocType(ABC):
    attributes: List['Attribute'] = field(default_factory=lambda: [])

    def __str__(self) -> str:
        return ""

    def __repr__(self) -> str:
        return str(self)

    def get_name(self) -> str:
        return None

    def get_c_name(self) -> str:
        return self.get_name()

    def get_unique_name(self) -> str:
        name = self.get_name()
        #print(type(self))
        assert name is not None
        return name

    def __hash__(self):
        return id(self)
    
    def __eq__(self, other):
        return self is other

@dataclass
class AttributeValue:
    value: Union['McdocType', List['AttributeValue']]
    name: Optional[str] = None

@dataclass
class Attribute:
    name: str
    value: Optional[AttributeValue] = None

@dataclass(eq=False)
class BuiltinType(McdocType):
    type: TypeKind
    c_name: str = None

    def __str__(self) -> str:
        return self.type.name.lower()

    def __repr__(self):
        return str(self)

    def get_name(self) -> str:
        return self.type.name.lower()

    def get_c_name(self) -> str:
        if self.c_name is None:
            return self.get_name()
        else:
            return self.c_name


@dataclass(eq=False)
class LiteralType(McdocType):
    parent: BuiltinType
    value: Union[str, float, int, bool]
    value_range: Optional[Range] = None

    def get_name(self) -> str:
        return f"Literal_{self.parent.get_unique_name()}_{self.value}" 

    def get_c_name(self) -> str:
        return self.parent.get_c_name()


@dataclass
class StructField:
    typ: McdocType
    attributes: List[Attribute]
    name: Optional[str] = None
    key_type: Optional[McdocType] = None
    spread: bool = False
    optional: bool = False
    # attributes

    def get_name(self) -> str:
        return name
    
@dataclass
class EnumField:
    name: str
    value: Union[int, float, str]

@dataclass(eq=False)
class StructType(McdocType):
    fields: List[StructField]
    name: Optional[str] = None

    def __str__(self) -> str:
        if self.name is None:
            return f"Anonymous Struct"
        return f"Struct '{self.name}'"

    def __repr__(self) -> str:
        return str(self)

    def get_name(self) -> str:
        return self.name

    def get_c_name(self) -> str:
        return f"struct {self.name}"

    def get_unique_name(self) -> str:
        name = self.get_name()
        if name is None:
            return f"Struct_{id(self)}"
        return name
            

@dataclass(eq=False)
class EnumType(McdocType):
    fields: List[EnumField]
    type: BuiltinType
    name: Optional[str] = None

    def __str__(self) -> str:
        if self.name is None:
            return f"Anonymous Enum"
        return f"Enum '{self.name}'"

    def __repr__(self) -> str:
        return str(self)

    def get_name(self) -> str:
        return self.name

    def get_c_name(self) -> str:
        return f"enum {self.name}"

    def get_unique_name(self) -> str:
        name = self.get_name()
        if name is None:
            return f"Enum_{id(self)}"
        return name

@dataclass
class ParamType(McdocType):
    name: str

    def __str__(self) -> str:
        return f"<{self.name}>"

    def __repr__(self):
        return str(self)

    def get_name(self) -> str:
        return self.name

    def __hash__(self):
        raise SyntaxError("prout")

    def __eq__(self, other):
        raise SyntaxError("prout")

@dataclass(eq=False)
class TypeAlias(McdocType):
    source: McdocType
    name: str
    params: List[str]
    # TODO: Handle type params!

    def __str__(self) -> str:
        return f"{self.name} -> {self.source}"

    def __repr__(self) -> str:
        return str(self)

    def get_name(self) -> str:
        return self.name

@dataclass(eq=False)
class UnionType(McdocType):
    elements: List[McdocType]

    def __str__(self) -> str:
        res = '('
        for e in self.elements[:-1]:
            res += f"{e} | "
        res += f"{self.elements[-1]})"
        return res

    def __repr__(self) -> str:
        return str(self)

    def get_unique_name(self) -> str:
        res = f"Union_{id(self)}"
        for e in self.elements:
            res += f"_{e.get_unique_name()}"
        return res


@dataclass(eq=False)
class TupleType(McdocType):
    elements: List[McdocType]

    def __str__(self) -> str:
        res = '['
        for e in self.elements[:-1]:
            res += f"{e}, "
        res += f"{self.elements[-1]}]"
        return res

    def __repr__(self) -> str:
        return str(self)

    def get_unique_name(self) -> str:
        res = f"Tuple_{id(self)}"
        for e in self.elements:
            res += f"_{e.get_unique_name()}"
        return res


@dataclass(eq=False)
class ListType(McdocType):
    elem_type: McdocType
    size_range: Optional[Union[Range, int]] = None

    def __str__(self):
        return f"[{self.elem_type}]{f" @ {self.size_range}" if self.size_range else ''}"

    def __repr__(self):
        return self.__str__()

    def get_unique_name(self) -> str:
        return f"List_{id(self)}_{self.elem_type.get_unique_name()}"


@dataclass(eq=False)
class ArrayType(McdocType):
    elem_type: BuiltinType
    size_range: Optional[Union[Range, int]] = None

    def __str__(self):
        return f"{self.elem_type}[]{f" @ {self.size_range}" if self.size_range else ''}"

    def __repr__(self):
        return self.__str__()

    def get_unique_name(self) -> str:
        return f"Array_{id(self)}_{self.elem_type.get_unique_name()}"


@dataclass(eq=False)
class TypeRef(McdocType):
    target: 'Path'

    def __str__(self):
        return str(self.target)

    def __repr__(self):
        return self.__str__()

    def get_name(self) -> str:
        return str(self.target)

@dataclass(eq=False)
class DispatcherType(McdocType):
    source: ResourceID
    indices: List[TypeIndex]

    def __str__(self):
        return f"{self.source}{self.indices}"

    def __repr__(self):
        return self.__str__()

    def get_unique_name(self) -> str:
        return str(self)

@dataclass(eq=False)
class GenericTypeInstance(McdocType):
    source: TypeAlias
    args: List[McdocType]

    def __str__(self):
        res = f"{self.source}<"
        if not self.args:
            return res + '>'
        for e in self.args[:-1]:
            res += f"{e}, "
        res += f"{self.args[-1]}>"
        return res

    def __repr__(self):
        return self.__str__()

    def get_name(self) -> str:
        res = self.source.get_name()
        for a in self.args:
            res += f"_{a.get_name()}"
        return res


