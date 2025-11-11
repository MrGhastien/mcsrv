from abc import ABC, abstractmethod
from dataclasses import dataclass
from mcdoc_common import *

from typing import List, Tuple

from mcdoc_types import TypeKind

class SyntaxError(Exception):
    def __init__(self, message: str, token=None):
        self.message = message
        self.token = token
        if token:
            super().__init__(f"{message} at line {token.line}, column {token.column}")
        else:
            super().__init__(message)

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

    def expect(self, expected: TokenType) -> Token:
        t:Token = self.peek()
        if t is None:
            SyntaxError(f"Expected {expected}, got EOF")
        if t.type == expected:
            return self.next()
        else:
            msg: str
            if t.value is not None:
                msg = f"Unexpected token: Expected {expected} but got {t.type} (with value '{t.value}')"
            else:
                msg = f"Unexpected token: Expected {expected} but got {t.type}"
            raise SyntaxError(msg, t)

    def prev(self) -> Optional[Token]:
        if self.pos == 0:
            return None
        self.pos -= 1
        return self.tokens[self.pos]

    def at_end(self) -> bool:
        return self.pos >= len(self.tokens)


class ASTNode(ABC):

    @abstractmethod
    def __str__(self):
        pass
# Basic nodes
@dataclass
class Identifier:
    name: str

    def __str__(self) -> str:
        return name

@dataclass
class Range(ASTNode):
    floating: bool
    start: Optional[Union[int, float]] = None
    end: Optional[Union[int, float]] = None
    start_exclusive: bool = False
    end_exclusive: bool = False



class TreeBody:
    values = []

class TreeValue:
    name: Optional[Token]
    value: TreeBody

@dataclass
class Attribute:
    name: Identifier
    value: List[TreeValue] = None

# Type nodes
@dataclass
class UnattrTypeNode(ASTNode, ABC):
    kind: TypeKind

@dataclass
class McdocTypeNode(ASTNode):
    unattr_type: UnattrTypeNode
    attributes: List[Attribute] = None
    indices: List[List[Token]] = None

    def __str__(self) -> str:
        return string(self.unattr_type)
    
@dataclass
class UnattrTypeRefNode(UnattrTypeNode):
    segments: List[Union[Identifier, TokenType]]
    absolute: bool = False

    def __str__(self) -> str:
        res = ""
        if self.absolute:
            res = "::"
        for i in range(len(self.segments) - 1):
            res += f"{self.segments[i]}::"
        res += string(self.segments[len(self.segments) - 1])
        return res
            

@dataclass
class UnattrSimpleTypeNode(UnattrTypeNode):
    literal = None
    value_range: Optional[Range] = None

    def __str__(self) -> str:
        return string(self.kind)

@dataclass
class UnattrArrayTypeNode(UnattrTypeNode):
    array_element_kind: TypeKind
    array_size_range: Optional[Range] = None

@dataclass
class UnattrListTypeNode(UnattrTypeNode):
    elem_type: McdocTypeNode
    size_range: Optional[Range] = None

@dataclass
class UnattrCompositeTypeNode(UnattrTypeNode):
    elements: List[McdocTypeNode] = None

@dataclass
class UnattrEnumTypeNode(UnattrTypeNode):
    name: Identifier
    value_kind: TypeKind
    fields: List[Tuple[Identifier, Union[int, float, str]]]

@dataclass
class StructField:
    key: Identifier
    optional: bool
    spread: bool
    attributes: List[Attribute]
    type: McdocTypeNode

@dataclass
class UnattrStructTypeNode(UnattrTypeNode):
    fields: List[StructField]
    name: Identifier

    def __str__(self) -> str:
        res = "struct"
        if name: res += f" {name}"
        res += '{'

        for f in fields:
            res += string(field)
        res += '}'

def analyze_path(ctx: ParseCtx, scanner: TokenScanner) -> Optional[UnattrTypeRefNode]:
    absolute = scanner.match(TokenType.DOUBLE_COLON)
    segments = []
    t = scanner.peek()
    if t.type is TokenType.KW_SUPER:
        segments.append(TokenType.KW_SUPER)
    elif t.type is TokenType.IDENTIFIER:
        segments.append(Identifier(t.value))
    else:
        raise SyntaxError(f"Unexpected token: {t.type}", t)
    scanner.next()

    while (t := scanner.peek()) and t.type == TokenType.DOUBLE_COLON:
        scanner.next()
        t = scanner.peek()
        if t.type is TokenType.KW_SUPER:
            segments.append(TokenType.KW_SUPER)
        elif t.type is TokenType.IDENTIFIER:
            segments.append(Identifier(t.value))
        else:
            raise SyntaxError(f"Unexpected token: {t}", t)
        scanner.next()
    return UnattrTypeRefNode(TypeKind.ANY, segments, absolute=absolute)
    

def analyze_attribute_value(ctx: ParseCtx, scanner: TokenScanner):
     t = scanner.peek()
     if t.type is TokenType.RPAREN or t.type is TokenType.RBRACKET or t.type is TokenType.RBRACE:
         analyze_attribute_tree_value(ctx, scanner)

def analyze_range(ctx: ParseCtx, scanner: TokenScanner) -> Range:
    t = scanner.peek()
    floating: bool = False
    start: Union[int, float]
    if t.type == TokenType.INTEGER:
        floating = False
    elif t.type == TokenType.FLOAT:
        floating = True
    else:
        raise SyntaxError("Invalid range type", t)

    start = t.value
    scanner.next()
    
    start_exclusive = False
    if scanner.match(TokenType.LCHEVRON):
        start_exclusive = True

    scanner.expect(TokenType.RANGE)

    end_exclusive = False
    if scanner.match(TokenType.LCHEVRON):
        end_exclusive = True
    
    t = scanner.next()
    if (floating and t.type is not TokenType.FLOAT) or (not floating and t.type is not TokenType.INTEGER):
        raise SyntaxError("Mismatched types of range ends", t)

    end = t.value

    return Range(floating, start, end, start_exclusive, end_exclusive)

def analyze_ranged_type(ctx: ParseCtx, scanner: TokenScanner) -> Range:
    if scanner.match(TokenType.AT):
        return analyze_range(ctx, scanner)
    return None

def analyze_array_type(ctx: ParseCtx, scanner: TokenScanner) -> Tuple[Optional[bool], Optional[Range]]:
    array_size_range: Range = None
    if scanner.match(TokenType.LBRACKET):
        scanner.expect(TokenType.RBRACKET)

        if scanner.match(TokenType.AT):
            array_size_range = analyze_range(ctx, scanner)
            if not array_size_range or array_size_range.floating:
                raise SyntaxError("Array range must be an int range.")
        return (True, array_size_range)
    else:
        return (False, None)

def analyze_enum_field(ctx: ParseCtx, scanner: TokenScanner) -> Tuple[Identifier, Union[int, float, str]]:
    t = scanner.peek()
    if t.type is not TokenType.IDENTIFIER:
        raise 

    scanner.next()
    id = Identifier(t.value)

    scanner.expect(TokenType.EQ)

    match (t := scanner.peek()):
        case TokenType.INTEGER:
            pass
        case TokenType.FLOAT:
            pass
        case TokenType.STRING:
            pass
        case _:
            raise SyntaxError(t)

    value = t.value;
    scanner.next()

    return (id, value)


def analyze_enum(ctx: ParseCtx, scanner: TokenScanner) -> Optional[UnattrCompositeTypeNode]:
    scanner.expect(TokenType.KW_ENUM)

    scanner.expect(TokenType.LPAREN)

    kind = None
    t = scanner.peek()
    match t.type:
        case TokenType.KW_BYTE:
            kind = TypeKind.BYTE
        case TokenType.KW_SHORT:
            kind = TypeKind.SHORT
        case TokenType.KW_INT:
            kind = TypeKind.INT
        case TokenType.KW_LONG:
            kind = TypeKind.LONG
        case TokenType.KW_FLOAT:
            kind = TypeKind.FLOAT
        case TokenType.KW_DOUBLE:
            kind = TypeKind.DOUBLE
        case TokenType.KW_STRING:
            kind = TypeKind.STRING
        case _:
            return None

    scanner.expect(TokenType.RPAREN)

    t = scanner.peek()
    id: Optional[Identifier]
    if t.type == TokenType.Identifier:
        id = Identifier(t.value)
        scanner.next()

    scanner.expect(TokenType.LBRACE)

    fields = [analyze_enum_field(ctx, scanner)]
    if not fields[0]:
        return None
    while t := scanner.peek() and t.type == TokenType.COMMA:
        scanner.next()
        if (u := scanner.peek()) and u.type == TokenType.RBRACE:
            break
        res = analyze_enum_field(ctx, scanner)
        if not res:
            return None
        fields.append(res)

    if not scanner.match(TokenType.RBRACE):
        return NONE

    return UnattrEnumTypeNode(TypeKind.ENUM, value_kind=kind, fields=fields)
        

def analyze_unattributed_type(ctx: ParseCtx, scanner:TokenScanner) -> Optional[UnattrTypeNode]:
    literal = None
    kind: TypeKind
    t = scanner.next()
    kind = TypeKind.from_token_type(t.type)
    match t.type:
        case TokenType.KW_BYTE | TokenType.KW_INT | TokenType.KW_LONG:
            value_range = analyze_ranged_type(ctx, scanner)
            is_array, array_size_range = analyze_array_type(ctx, scanner);
            
            if is_array is None:
                return None
            elif is_array:
                return UnattrArrayTypeNode(TypeKind.ARRAY, value_range=value_range, array_element_kind=kind, array_size_range=array_size_range)
            else:
                return UnattrTypeNode(kind, value_range=value_range)
        case TokenType.KW_SHORT | TokenType.KW_FLOAT | TokenType.KW_DOUBLE | TokenType.KW_STRING:
            value_range = analyze_ranged_type(ctx, scanner)
            return UnattrSimpleTypeNode(kind, value_range=value_range)
        case TokenType.KW_ANY | TokenType.KW_BOOLEAN:
            return UnattrArrayTypeNode(kind)
        case TokenType.LPAREN:
            # Union
            elems = [analyze_type(ctx, scanner)]
            if not elems[0]:
                return None
            while t := scanner.peek() and t.type == TokenType.PIPE:
                scanner.next()
                if (u := scanner.peek()) and u.type == TokenType.RPAREN:
                    break
                typ = analyze_type(ctx, scanner)
                if not typ:
                    return None
                elems.append(typ)
            if not scanner.match(TokenType.RPAREN):
                return None
            return UnattrTypeNode(TypeKind.UNION, elements=elems)

        case TokenType.LBRACKET:
            # List or Tuple
            trailing_comma = False
            elems = [analyze_type(ctx, scanner)]
            while (t:= scanner.peek()) and t.type == TokenType.COMMA:
                scanner.next()
                if (u := scanner.peek()) and u.type == TokenType.RBRACKET:
                    trailing_comma = True
                    break
                typ = analyze_type(ctx, scanner)
                if not typ:
                    return None
                elems.append(typ)
            if not scanner.match(TokenType.RBRACKET):
                return None
            if len(elems) == 1 and trailing_comma:
                # List !
                size_range = analyze_ranged_type(ctx, scanner)
                return UnattrListTypeNode(TypeKind.LIST, elem_type=elems[0], size_range=size_range)
            return UnattrTypeNode(TypeKind.TUPLE, elements=elems)
        case TokenType.KW_SUPER | TokenType.DOUBLE_COLON | TokenType.IDENTIFIER:
            scanner.prev()
            return analyze_path(ctx, scanner)
        case TokenType.RESID:
            # Dispatcher
            print("DISPATCHER TODO")
            pass
        case TokenType.KW_ENUM:
            scanner.prev()
            return analyze_enum(ctx, scanner)
        case TokenType.KW_STRUCT:
            scanner.prev()
            return analyze_struct(ctx, scanner)
            pass
        case TokenType.KW_FALSE | TokenType.KW_TRUE:
            return UnattrSimpleTypeNode(TypeKind.BOOLEAN, literal=(t.type == TokenType.KW_TRUE))
        case TokenType.STRING | TokenType.INTEGER | TokenType.FLOAT:
            return UnattrSimpleTypeNode(TypeKind.from_token_type(t.type), literal=t.value)




def analyze_type(ctx: ParseCtx, scanner: TokenScanner):
    attrs: List = None
    if (t := scanner.peek()) and t.type == TokenType.ATTR_BEGIN:
        attrs = analyze_attributes(ctx, scanner)
        if not attrs:
            raise SyntaxError("", t)

    unattr = analyze_unattributed_type(ctx, scanner)

    return McdocTypeNode(unattr, attributes=attrs)
        
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
             passpapass
        
    
def analyze_attribute_tree_value(ctx: ParseCtx, scanner: TokenScanner) -> TreeValue:
    t = scanner.peek()
    match t.type:
        case TokenType.LPAREN:
            analyze_attribute_tree_body(ctx, scanner)
            scanner.expect(TokenType.RPAREN)
        case TokenType.LBRACKET:
            analyze_attribute_tree_body(ctx, scanner)
            scanner.expect(TokenType.RBRACKET)
        case TokenType.LBRACE:
            analyze_attribute_tree_body(ctx, scanner)
            scanner.expect(TokenType.RBRACE)
    return None


def analyze_attribute(ctx: ParseCtx, scanner: TokenScanner, list: List[Attribute]):
    scanner.expect(TokenType.ATTR_BEGIN)

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
              pass
        case TokenType.LPAREN:
            analyze_attribute_tree_body(ctx, scanner)
            scanner.expect(TokenType.RPAREN)
        case TokenType.LBRACKET:
            analyze_attribute_tree_body(ctx, scanner)
            scanner.expect(TokenType.RBRACKET)
        case TokenType.LBRACE:
            analyze_attribute_tree_body(ctx, scanner)
            scanner.expect(TokenType.RBRACE)
        case _:
            return False


def analyze_attributes(ctx: ParseCtx, scanner: TokenScanner):
    list = []
    while (t := scanner.peek()) and t.type == TokenType.ATTR_BEGIN:
        if not analyze_attribute(ctx, scanner, list):
            return None
    return list

def analyze_struct_field(ctx: ParseCtx, scanner: TokenScanner) -> Optional[StructField]:
    key_tok = scanner.peek()
    key: Union[str, Identifier, McdocTypeNode] = None
    optional: bool = False
    match key_tok.type:
        case TokenType.STRING:
            scanner.next()
            key = key_tok.value
        case TokenType.IDENTIFIER:
            scanner.next()
            key = Identifier(key_tok.value)
        case TokenType.LBRACKET:
            scanner.next()
            key = analyze_type(ctx, scanner)
            scanner.expect(TokenType.RBRACKET)
        case _:
            raise SyntaxError(" ", key_tok)
    if scanner.match(TokenType.QUESTION):
        optional = True

    scanner.expect(TokenType.COLON)

    # attributes: List[Attribute] = []
    # if (t := scanner.peek()) and t.type == TokenType.ATTR_BEGIN:
    #     if not analyze_attributes(ctx, scanner, attributes) or not scanner.match(TokenType.SPREAD):
    #         return None

    type: McdocType = analyze_type(ctx, scanner)

    return StructField(key, optional, False, [], type)

def analyze_struct(ctx: ParseCtx, scanner: TokenScanner):
    if not scanner.match(TokenType.KW_STRUCT):
        return False

    id: Identifier = None
    struct: Struct
    id_tok = scanner.peek()
    if not id_tok:
        return None
    if id_tok.type == TokenType.IDENTIFIER:
        scanner.next()
        id = Identifier(id_tok.value)

    scanner.expect(TokenType.LBRACE)

    if scanner.match(TokenType.RBRACE):
        return UnattrStructTypeNode(kind=TypeKind.STRUCT, fields=[], name=id)

    fields = [analyze_struct_field(ctx, scanner)]
    if not fields[0]:
        raise SyntaxError(scanner.peek())
    while (t := scanner.peek()) and t.type == TokenType.COMMA:
        scanner.next()
        if (u := scanner.peek()) and u.type == TokenType.RBRACE:
            break
        res = analyze_struct_field(ctx, scanner)
        if not res:
            return None
        fields.append(res)

    scanner.expect(TokenType.RBRACE)

    return UnattrStructTypeNode(kind=TypeKind.STRUCT, fields=fields, name=id)
    
def analyze(ctx: ParseCtx):
    scanner: TokenScanner = TokenScanner(ctx.tokens)

    things = []
    while not scanner.at_end():
        t = scanner.peek()
        match t.type:
            case TokenType.KW_STRUCT:
                things.append(analyze_struct(ctx, scanner))
            case TokenType.KW_ENUM:
                things.append(analyze_enum(ctx, scanner))
            case _:
                return None

    print(things)
