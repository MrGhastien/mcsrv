from abc import ABC, abstractmethod
from dataclasses import dataclass
from mcdoc_common import *

class TokenScanner:
    tokens: List[Token]
    pos: int

    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0

    def next(self) -> Optional[Token]:
        if self.pos >= len(self.tokens):
            return None
        t = self.tokens[self.pos]
        self.pos += 1
        return t

    def peek(self, offset: int = 0) -> Optional[Token]:
        peek_pos = self.pos + offset
        if peek_pos >= len(self.tokens):
            return None
        return self.tokens[peek_pos]

    def match(self, expected: TokenType, value=None) -> bool:
        t:Token = self.peek()
        if t.type == expected and t.value == value:
            self.next()
            return True;
        else:
            return False

    def prev(self) -> Optional[Token]:
        if self.pos == 0:
            return None
        return self.tokens[self.pos - 1]

    def at_end(self) -> bool:
        return self.pos >= len(self.tokens)


class ASTNode(ABC):

    @abstractmethod
    def __str__(self):
        pass

@dataclass
class Identifier:
    name: str

class TreeValue:
    name: Optional[Token]
    value: Union[TreeBody, McdocType]

@dataclass
class Attribute:
    name: Identifier
    value: List[TreeValue] = None

class McdocType:
    attributes: List[Attribute]
    ref: int # TODO
    indices: List[Union[Token, List[Token]]]

@dataclass
class Field(ASTNode):
    key: Identifier
    optional: bool
    spread: bool
    attributes: List[Attribute]
    type: McdocType

@dataclass
class Struct(ASTNode):
    name: Identifier
    fields: List[Field] = []

    def __str__(self):
        return 'struct'

def analyze_attribute_value(ctx: ParseCtx, scanner: TokenScanner):
     t = scanner.peek()
     if t.type is TokenType.RPAREN or t.type is TokenType.RBRACKET or t.type is TokenType.RBRACE:
         analyze_attribute_tree_value(ctx, scanner)

def analyze_type(ctx: ParseCtx, scanner: TokenScanner):
    if (t := scanner.peek()) and t.type == TokenType.ATTR_BEGIN:
        if not analyze_attributes(ctx, scanner):
            return None

    t = scanner.peek()
    match t.type:
        case TokenType.KW_ANY | TokenType.KW_BOOLEAN:
            pass
        case TokenType.KW_STRING:
            pass
        case TokenType.STRING | TokenType.INTEGER | TokenType.FLOAT | TokenType.KW_FALSE | TokenType.KW_TRUE:
            pass
        case TokenType.KW_BYTE | TokenType.KW_SHORT | TokenType.KW_INT | TokenType.KW_LONG | TokenType.KW_FLOAT | TokenType.KW_DOUBLE:
            pass
    
def analyze_attribute_tree_body(ctx: ParseCtx, scanner: TokenScanner) -> TreeValue:
     t = scanner.peek()
     match t.type:
         case TokenType.LPAREN | TokenType.LBRACKET | TokenType.LBRACE:
             # Value
             pass
         case TokenType.IDENTIFIER | TokenType.STRING:
             t2 = scanner.peek(1)
             if t2 and t2.type == TokenType.EQ:
                 # Named value
                 pass
             else:
                 # Value (Type in grammar)
                 pass
         case _:
             # Value (Type in grammar)
                 
        
    
def analyze_attribute_tree_value(ctx: ParseCtx, scanner: TokenScanner) -> TreeValue:
    t = scanner.peek()
    match t.type:
        case TokenType.LPAREN:
            analyze_attribute_tree_body(ctx, scanner)
            if not scanner.match(TokenType.RPAREN):
                return False
        case TokenType.LBRACKET:
            analyze_attribute_tree_body(ctx, scanner)
            if not scanner.match(TokenType.RBRACKET):
                return False
        case TokenType.LBRACE:
            analyze_attribute_tree_body(ctx, scanner)
            if not scanner.match(TokenType.RBRACE):
                return False
    return True


def analyze_attribute(ctx: ParseCtx, scanner: TokenScanner, list: List[Attribute]):
    if not scanner.match(TokenType.ATTR_BEGIN):
        return False;

    id: Identifier
    if (t := scanner.next()) and t.type == TokenType.IDENTIFIER:
        id = Identifier(t.value)
    else:
        return False

    if scanner.match(TokenType.RBRACKET):
        list.append(Attribute(id))
        return True

    tok = scanner.peek()
    match tok.type:
        case TokenType.EQ:
              
        case TokenType.LPAREN:
            analyze_attribute_tree_body(ctx, scanner)
            if not scanner.match(TokenType.RPAREN):
                return False
        case TokenType.LBRACKET:
            analyze_attribute_tree_body(ctx, scanner)
            if not scanner.match(TokenType.RBRACKET):
                return False
        case TokenType.LBRACE:
            analyze_attribute_tree_body(ctx, scanner)
            if not scanner.match(TokenType.RBRACE):
                return False

        case _:
            return False


def analyze_attributes(ctx: ParseCtx, scanner: TokenScanner, list: List[Attribute]):
    while (t := scanner.peek()) and t.type == TokenType.ATTR_BEGIN:
        if not anayze_attribute(ctx, scanner, list):
            return False
    return True

def analyze_field(ctx: ParseCtx, scanner: TokenScanner, struct: Struct):
    key_tok = scanner.peek()
    key: Union[str, Identifier, Type] = None
    optional: bool = False
    match key_tok.type:
        case TokenType.STRING:
            key = key_tok.value
        case TokenType.IDENTIFIER:
            key = Identifier(key_tok.value)
        case TokenType.LBRACKET:
            key = analyze_type(ctx, scanner)
            if not scanner.match(TokenType.RBRACKET):
                return False
    if scanner.match(TokenType.QUESTION):
        optional = True

    attributes: List[Attribute]
    if (t := scanner.peek()) and t.type == TokenType.ATTR_BEGIN:
        if not analyze_attributes(ctx, scanner, attributes) or not scanner.match(TokenType.SPREAD):
            return False

    type: McdocType = analyze_type(ctx, scanner)

    struct.fields.append(Field(key, optional, len(attributes) > 0, attributes, type))
    
    return True

def analyze_struct(ctx: ParseCtx, scanner: TokenScanner):
    if not scanner.match(TokenType.KW_STRUCT):
        return False

    id: Identifier
    struct: Struct
    id_tok = scanner.peek()
    if id_tok and id_tok.type == TokenType.IDENTIFIER:
        scanner.next()
        struct = Struct(name=Identifier(id_tok.value))

    if not scanner.match(TokenType.LBRACE):
        return False

    analyze_field(ctx, scanner, struct)

    while scanner.match(TokenType.COMMA):
        analyze_field(ctx, scanner, struct)

    if not scanner.match(TokenType.RBRACE):
        return False

    
def analyze(ctx: ParseCtx):
    scanner: TokenScanner = TokenScanner(ctx.tokens)
