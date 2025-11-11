from enum import Enum, auto
from dataclasses import dataclass
from typing import Optional, Union, List, TextIO

class ResourceID:
    def __init__(self, namespace, path):
        self.namespace = namespace
        self.path = path

class TokenType(Enum):
    NONE = None
    INTEGER = 1
    FLOAT = 2
    STRING = 3
    RESID = 4
    IDENTIFIER = 5
    LPAREN, = '('
    RPAREN = ')'
    LBRACE = '{'
    RBRACE = '}'
    LBRACKET = '['
    RBRACKET = ']'
    LCHEVRON = '<'
    RCHEVRON = '>'
    COMMA = ','
    COLON = ':'
    PIPE = '|'
    QUESTION = '?'
    AT = '@'
    EQ = '='
    DOT = '.'

    RANGE = ".."
    DOUBLE_COLON = "::"
    ATTR_BEGIN = "#["

    SPREAD = "..."

    KW_ANY = "any"
    KW_BYTE = "byte"
    KW_SHORT = "short"
    KW_INT = "int"
    KW_LONG = "long"
    KW_FLOAT = "float"
    KW_DOUBLE = "double"
    KW_STRING = "string"
    KW_FALSE = "false"
    KW_TRUE = "true"
    KW_BOOLEAN = "boolean"
    KW_ENUM = "enum"
    KW_STRUCT = "struct"
    KW_FALLBACK = "%fallback"
    KW_NONE = "%none"
    KW_UNKNOWN = "%unknown"
    KW_KEY = "%key"
    KW_PARENT = "%parent"
    KW_TYPE = "type"
    KW_USE = "use"
    KW_AS = "as"
    KW_INJECT = "inject"
    KW_DISPATCH = "dispatch"
    KW_TO = "to"
    KW_SUPER = "super"

    def is_keyword(self) -> bool:
        return self.value in keywords


keywords = {
    "any": TokenType.KW_ANY,
    "byte": TokenType.KW_BYTE,
    "short": TokenType.KW_SHORT,
    "int": TokenType.KW_INT,
    "long": TokenType.KW_LONG,
    "float": TokenType.KW_FLOAT,
    "double": TokenType.KW_DOUBLE,
    "string": TokenType.KW_STRING,
    "false": TokenType.KW_FALSE,
    "true": TokenType.KW_TRUE,
    "boolean": TokenType.KW_BOOLEAN,
    "enum": TokenType.KW_ENUM,
    "struct": TokenType.KW_STRUCT,
    "%fallback": TokenType.KW_FALLBACK,
    "%none": TokenType.KW_NONE,
    "%unknown": TokenType.KW_UNKNOWN,
    "%key": TokenType.KW_KEY,
    "%parent": TokenType.KW_PARENT,
    "type": TokenType.KW_TYPE,
    "use": TokenType.KW_USE,
    "as": TokenType.KW_AS,
    "inject": TokenType.KW_INJECT,
    "dispatch": TokenType.KW_DISPATCH,
    "to": TokenType.KW_TO,
    "super": TokenType.KW_SUPER
}

@dataclass
class ResourceID:
    namespace: str
    path: str

@dataclass
class Token:
    type: TokenType
    line: int
    column: int
    value: Optional[Union[str, int, float, ResourceID]] = None

    def __str__(self):
        """Pour print() et str()"""
        # if self.value:
        #     return f"({self.type.name}: '{self.value}')"
        # return f"{self.type.name}"

        if self.value:
            return f"'{self.value}'"
        return f"{self.type.value}"
    
    def __repr__(self):
        """Pour la console interactive et debugging"""
        return self.__str__()

class Scanner:
    def __init__(self, filename: str):
        with open(filename, 'r', encoding='utf-8') as f:
            self.text = f.read()
        self.pos = 0
        self.__line = 1
        self.__column = 1

    def next_char(self) -> Optional[str]:
        c = self.text[self.pos]
        if c == '\n':
            self.__line += 1
            self.__column = 1
        else:
            self.__column += 1
        self.pos += 1
        return c

    def peek(self, offset: int = 0) -> Optional[str]:
        peek_pos = self.pos + offset
        if peek_pos >= len(self.text):
            return None
        return self.text[peek_pos]

    def match(self, expected) -> bool:
        c = self.peek()
        if c == expected:
            self.next_char()
            return True;
        else:
            return False

    def prev_char(self) -> Optional[str]:
        if self.pos == 0:
            return None
        return self.text[self.pos - 1]

    def at_end(self) -> bool:
        return self.pos >= len(self.text)
        

    @property
    def line(self):
        return self.__line

    @property
    def column(self):
        return self.__column

class ParseCtx:
    def __init__(self, input: Scanner):
        self.tokens: List[Token] = []
        self.start = 0
        self.has_error = False
        self.input = input

    def add_token(self, tok_type: TokenType):
        self.tokens.append(Token(tok_type, self.input.line, self.input.column))

    def add_generic_token(self, tok_type: TokenType, value: Union[str, int, float, ResourceID]):
        self.tokens.append(Token(tok_type, self.input.line, self.input.column, value=value))

    def lexeme(self):
        return self.input.text[self.start:self.input.pos]
