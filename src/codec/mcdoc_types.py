from mcdoc_common import TokenType
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
        return f"{self.start}{'<' if self.start_exclusive else ''}..{'<' if self.end_exclusive else ''}{self.end}"


@dataclass(kw_only=True, repr=False)
class McdocType(ABC):
    attributes: List['Attribute'] = field(default_factory=lambda: [])

    def __str__(self) -> str:
        pass

    def __repr__(self) -> str:
        return str(self)

@dataclass
class AttributeValue:
    value: Union['McdocType', List['AttributeValue']]
    name: Optional[str] = None

@dataclass
class Attribute:
    name: str
    value: Optional[AttributeValue] = None

@dataclass
class BuiltinType(McdocType):
    type: TypeKind

@dataclass
class LiteralType(BuiltinType):
    value: Union[str, float, int, bool]

@dataclass
class StructField:
    typ: McdocType
    attributes: List[Attribute]
    name: Optional[str] = None
    key_type: Optional[McdocType] = None
    # attributes
    
@dataclass
class EnumField:
    name: str
    value: Union[int, float, str]

@dataclass
class StructType(McdocType):
    fields: List[StructField]
    name: Optional[str] = None

    def __str__(self) -> str:
        if self.name is None:
            return f"Anonymous Struct"
        return f"Struct '{self.name}'"

    def __repr__(self) -> str:
        return str(self)

            

@dataclass
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



@dataclass
class TypeAlias(McdocType):
    source: McdocType

@dataclass
class UnionType(McdocType):
    elements: List[McdocType]

@dataclass
class TupleType(McdocType):
    elements: List[McdocType]

@dataclass
class ListType(McdocType):
    elem_type: McdocType
    size_range: Optional[Range] = None

@dataclass
class ArrayType(McdocType):
    elem_type: BuiltinType
    size_range: Optional[Range] = None
