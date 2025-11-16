from mcdoc_common import TokenType
from enum import Enum, auto
from abstract import ABC

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
class McdocType(ABC):
    pass

@dataclass
class StructField:
    name: Optional[str] = None
    key_type: Optional[McdocType] = None
    typ: McdocType
    # attributes
    

@dataclass
class StructType(McdocType):
    fields: List[StructField]
    name: Optional[str] = None
