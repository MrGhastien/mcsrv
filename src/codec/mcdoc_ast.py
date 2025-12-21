from mcdoc_common import *
from mcdoc_ast_nodes import *

from typing import List, Tuple, Optional

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
        if t is not None and t.type == expected and t.value == value:
            self.next()
            return True;
        else:
            return False

    def match_peek(self, expected: TokenType, offset: int = 0) -> bool:
        t:Token = self.peek(offset)
        return t.type == expected

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

    def backtrack(self, new_pos: int):
        if new_pos >= self.pos:
            return
        self.pos = new_pos

    def at_end(self) -> bool:
        return self.pos >= len(self.tokens)

def is_valid_list_range(r) -> bool:
    match r:
        case Range() as rng:
            return not rng.floating
        case int():
            return True
        case _:
            return False


def can_be_identifier(tok: Token) -> bool:
    match tok.type:
        case TokenType.IDENTIFIER | TokenType.KW_STRING:
            return True
        case TokenType.KW_TYPE | TokenType.KW_USE | TokenType.KW_AS | TokenType.KW_INJECT |TokenType.KW_DISPATCH | TokenType.KW_TO:
            return True
        case _:
            return False

def analyze_identifier(ctx: ParseCtx, scanner: TokenScanner) -> Identifier:
    t = scanner.next()
    match t.type:
        case TokenType.IDENTIFIER | TokenType.KW_STRING:
            return Identifier(t.value)
        case TokenType.KW_TYPE | TokenType.KW_USE | TokenType.KW_AS | TokenType.KW_INJECT |TokenType.KW_DISPATCH | TokenType.KW_TO:
            # Convert the keyword to an indentifier, as these ones are not reserved
            return Identifier(t.type.value)
        case _:
            raise SyntaxError(f"Token {t} ({t.type}) is not an identifier", t)

def analyze_path(ctx: ParseCtx, scanner: TokenScanner) -> Path:
    absolute = scanner.match(TokenType.DOUBLE_COLON)
    segments = []
    t = scanner.peek()
    if t.type is TokenType.KW_SUPER:
        scanner.next()
        segments.append(TokenType.KW_SUPER)
    else:
        segments.append(analyze_identifier(ctx, scanner))

    while (t := scanner.peek()) and t.type == TokenType.DOUBLE_COLON:
        scanner.next()
        t = scanner.peek()
        if t.type is TokenType.KW_SUPER:
            scanner.next()
            segments.append(TokenType.KW_SUPER)
        else:
            segments.append(analyze_identifier(ctx, scanner))
    return Path(segments, absolute=absolute)
    

def analyze_attribute_value(ctx: ParseCtx, scanner: TokenScanner):
     t = scanner.peek()
     if t.type is TokenType.RPAREN or t.type is TokenType.RBRACKET or t.type is TokenType.RBRACE:
         analyze_attribute_value(ctx, scanner)

def analyze_range(ctx: ParseCtx, scanner: TokenScanner) -> Union[Range, int, float]:
    t = scanner.peek()
    floating: bool = False
    start: Optional[Union[int, float]]
    match t.type:
        case TokenType.INTEGER | TokenType.FLOAT:
            scanner.next()
            floating = t.type is TokenType.FLOAT
            start = t.value
        case TokenType.RANGE:
            start = None
        case _:
            raise SyntaxError(f"Unexpected token '{t}' while parsing range.", t)
    
    start_exclusive = False
    if scanner.match(TokenType.LCHEVRON):
        if start is None:
            raise SyntaxError(f"Range start cannot be exclusive when there is no lower bound!", scanner.peek())
        start_exclusive = True

    if not scanner.match(TokenType.RANGE):
        return start

    end_exclusive = False
    if scanner.match(TokenType.LCHEVRON):
        end_exclusive = True
    
    end: Optional[Union[float, int]] = None
    if (t := scanner.peek()) is not None:
        match t.type:
            case TokenType.FLOAT:
                if not floating:
                    floating = True
                    start = float(start)
                scanner.next()
                end = t.value
            case TokenType.INTEGER:
                scanner.next()
                if floating:
                    end = float(t.value)
                else:
                    end = t.value
            case _:
                if end_exclusive:
                    raise SyntaxError(f"Range end cannot be exclusive when there is no upper bound!", t)
                end = None
        end = t.value

    return Range(floating, start, end, start_exclusive, end_exclusive)

def analyze_ranged_type(ctx: ParseCtx, scanner: TokenScanner) -> Optional[Union[Range, int, float]]:
    if scanner.match(TokenType.AT):
        return analyze_range(ctx, scanner)
    return None

def analyze_array_type(ctx: ParseCtx, scanner: TokenScanner) -> Tuple[Optional[bool], Optional[Range]]:
    array_size_range: Range = None
    if scanner.match(TokenType.LBRACKET):
        scanner.expect(TokenType.RBRACKET)

        if scanner.match(TokenType.AT):
            array_size_range = analyze_range(ctx, scanner)
            if not is_valid_list_range(array_size_range):
                raise SyntaxError("Array range must be an int range.")
        return (True, array_size_range)
    else:
        return (False, None)

def analyze_enum_field(ctx: ParseCtx, scanner: TokenScanner) -> Tuple[Identifier, Union[int, float, str]]:
    attrs = []
    if scanner.match_peek(TokenType.ATTR_BEGIN):
        attrs = analyze_attributes(ctx, scanner)
    id = analyze_identifier(ctx, scanner)

    scanner.expect(TokenType.EQ)

    t = scanner.peek();
    if t.type is not TokenType.INTEGER and t.type is not TokenType.STRING and t.type is not TokenType.FLOAT:
        raise SyntaxError(f"Enum field {id} is not of valid type: '{t.type}'.", t)
    value = t.value;
    scanner.next()

    return (id, value)


def analyze_enum(ctx: ParseCtx, scanner: TokenScanner) -> Optional[UnattrCompositeTypeNode]:
    scanner.expect(TokenType.KW_ENUM)

    scanner.expect(TokenType.LPAREN)

    kind = None
    t = scanner.next()
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
            raise SyntaxWarning(f"Invalid enum type '{t}'", t)

    scanner.expect(TokenType.RPAREN)

    id: Optional[Identifier] = None
    if can_be_identifier(scanner.peek()):
        id = analyze_identifier(ctx, scanner)

    scanner.expect(TokenType.LBRACE)

    fields = [analyze_enum_field(ctx, scanner)]
    while (t := scanner.peek()) and t.type == TokenType.COMMA:
        scanner.next()
        if (u := scanner.peek()) and u.type == TokenType.RBRACE:
            break
        res = analyze_enum_field(ctx, scanner)
        fields.append(res)

    scanner.expect(TokenType.RBRACE)

    return UnattrEnumTypeNode(TypeKind.ENUM, name=id, value_kind=kind, fields=fields)
        
def analyze_union(ctx: ParseCtx, scanner:TokenScanner) -> UnattrCompositeTypeNode:
    if scanner.match(TokenType.RPAREN):
        return UnattrCompositeTypeNode(TypeKind.UNION, elements=[])
    elems = [analyze_type(ctx, scanner)]
    while (t := scanner.peek()) and t.type == TokenType.PIPE:
        scanner.next()
        if (u := scanner.peek()) and u.type == TokenType.RPAREN:
            break
        typ = analyze_type(ctx, scanner)
        elems.append(typ)
    scanner.expect(TokenType.RPAREN)
    return UnattrCompositeTypeNode(TypeKind.UNION, elements=elems)

def analyze_list_or_tuple(ctx: ParseCtx, scanner:TokenScanner) -> Union[UnattrListTypeNode, UnattrCompositeTypeNode]:
    trailing_comma = False
    elems = [analyze_type(ctx, scanner)]
    while (t:= scanner.peek()) and t.type == TokenType.COMMA:
        scanner.next()
        if (u := scanner.peek()) and u.type == TokenType.RBRACKET:
            trailing_comma = True
            break
        typ = analyze_type(ctx, scanner)
        elems.append(typ)
    scanner.expect(TokenType.RBRACKET)
    if len(elems) == 1 and not trailing_comma:
        # List !
        size_range = analyze_ranged_type(ctx, scanner)

        if size_range is not None and not is_valid_list_range(size_range):
            raise SyntaxError("List must be an int range.")
        return UnattrListTypeNode(TypeKind.LIST, elem_type=elems[0], size_range=size_range)
    return UnattrCompositeTypeNode(TypeKind.TUPLE, elements=elems)

def analyze_unattributed_type(ctx: ParseCtx, scanner:TokenScanner) -> Optional[UnattrTypeNode]:
    kind: TypeKind
    t = scanner.next()
    kind = TypeKind.from_token_type(t.type)
    match t.type:
        case TokenType.KW_BYTE | TokenType.KW_INT | TokenType.KW_LONG:
            value_range = analyze_ranged_type(ctx, scanner)
            is_array, array_size_range = analyze_array_type(ctx, scanner);
            
            if is_array:
                return UnattrArrayTypeNode(TypeKind.ARRAY, value_range=value_range, array_element_kind=kind, array_size_range=array_size_range)
            else:
                return UnattrSimpleTypeNode(kind, value_range=value_range)
        case TokenType.KW_SHORT | TokenType.KW_FLOAT | TokenType.KW_DOUBLE | TokenType.KW_STRING:
            value_range = analyze_ranged_type(ctx, scanner)
            return UnattrSimpleTypeNode(kind, value_range=value_range)
        case TokenType.KW_ANY | TokenType.KW_BOOLEAN:
            return UnattrArrayTypeNode(TypeKind.ARRAY, kind)
        case TokenType.LPAREN:
            return analyze_union(ctx, scanner)
        case TokenType.LBRACKET:
            return analyze_list_or_tuple(ctx, scanner)
        case TokenType.KW_SUPER | TokenType.DOUBLE_COLON | TokenType.IDENTIFIER:
            scanner.prev()
            return UnattrTypeRefNode(TypeKind.ANY, path=analyze_path(ctx, scanner))
        case TokenType.RESID:
            source = t.value
            indices = analyze_type_indices(ctx, scanner)
            return UnattrDispatcherTypeNode(TypeKind.DISPATCHER, source=source, indices=indices)
        case TokenType.KW_ENUM:
            scanner.prev()
            return analyze_enum(ctx, scanner)
        case TokenType.KW_STRUCT:
            scanner.prev()
            return analyze_struct(ctx, scanner)
            pass
        case TokenType.KW_FALSE | TokenType.KW_TRUE:
            literal = t.type == TokenType.KW_TRUE
            return UnattrSimpleTypeNode(TypeKind.BOOLEAN, literal=literal)
        case TokenType.STRING | TokenType.INTEGER | TokenType.FLOAT:
            return UnattrSimpleTypeNode(TypeKind.from_token_type(t.type), literal=t.value)


def analyze_dynamic_type_index_segment(ctx: ParseCtx, scanner: TokenScanner):
    u = scanner.peek()
    match u.type:
        case TokenType.KW_KEY | TokenType.KW_PARENT:
            scanner.next()
            return u.type
        case TokenType.RESID:
            scanner.next()
            return u.value
        case _:
            if can_be_identifier(u):
                return analyze_identifier(ctx, scanner)
            else:
                raise SyntaxError(f"Unexpected token {u} while parsing dynamic type index")
    
def analyze_dynamic_type_index(ctx: ParseCtx, scanner: TokenScanner):
    scanner.expect(TokenType.LBRACKET)
    accessor_segments = [analyze_dynamic_type_index_segment(ctx, scanner)]
    while (t := scanner.peek()) and t.type is TokenType.DOT:
        scanner.next()
        accessor_segments.append(analyze_dynamic_type_index_segment(ctx, scanner))

    scanner.expect(TokenType.RBRACKET)
    return TypeIndex(accessor_segments, dynamic=True)
    
def analyze_type_index(ctx: ParseCtx, scanner: TokenScanner) -> TypeIndex:
    t = scanner.peek()
    match t.type:
        case TokenType.KW_FALLBACK | TokenType.KW_NONE | TokenType.KW_UNKNOWN:
            scanner.next()
            return TypeIndex(key = [t.type])
        case TokenType.RESID | TokenType.IDENTIFIER | TokenType.STRING:
            scanner.next()
            return TypeIndex(key = [t.value])
        case TokenType.LBRACKET:
            return analyze_dynamic_type_index(ctx, scanner)
        case _:
            raise SyntaxError(f"Unexpected token {t} while parsing type index", t)

def analyze_type_indices(ctx: ParseCtx, scanner: TokenScanner) -> List[TypeIndex]:
    scanner.expect(TokenType.LBRACKET);

    indices = [analyze_type_index(ctx, scanner)]

    while (t := scanner.peek()) and t.type is TokenType.COMMA:
        scanner.next()
        if (u := scanner.peek()) and u.type is TokenType.RBRACKET:
            break

        indices.append(analyze_type_index(ctx, scanner))

    scanner.expect(TokenType.RBRACKET);

    return indices

def analyze_type_args(ctx: ParseCtx, scanner: TokenScanner) -> List[McdocTypeNode]:
    scanner.expect(TokenType.LCHEVRON);

    args = [analyze_type(ctx, scanner)]

    while (t := scanner.peek()) and t.type is TokenType.COMMA:
        scanner.next()
        if (u := scanner.peek()) and u.type is TokenType.RCHEVRON:
            break

        args.append(analyze_type(ctx, scanner))

    scanner.expect(TokenType.RCHEVRON);
    return args

def analyze_type(ctx: ParseCtx, scanner: TokenScanner):
    attrs = analyze_attributes(ctx, scanner)

    unattr = analyze_unattributed_type(ctx, scanner)

    indices = []
    args = []
    while (t := scanner.peek()) and (t.type is TokenType.LCHEVRON or t.type is TokenType.LBRACKET):
        if t.type is TokenType.LCHEVRON:
            new_args = analyze_type_args(ctx, scanner)
            args.extend(new_args)
        else:
            new_indices = analyze_type_indices(ctx, scanner)
            indices.extend(new_indices)

    return McdocTypeNode(unattr, attributes=attrs)

def analyze_type_params(ctx: ParseCtx, scanner: TokenScanner) -> List[Identifier]:
    scanner.expect(TokenType.LCHEVRON);

    params = [analyze_type_index(ctx, scanner)]

    while (t := scanner.peek()) and t.type is TokenType.COMMA:
        scanner.next()
        if (u := scanner.peek()) and u.type is TokenType.RCHEVRON:
            break

        params.append(scanner.expect(TokenType.IDENTIFIER).value)

    scanner.expect(TokenType.RCHEVRON);
    return params


def analyze_type_alias(ctx: ParseCtx, scanner: TokenScanner) -> TypeAliasStatement:
    attrs = analyze_attributes(ctx, scanner)
    scanner.expect(TokenType.KW_TYPE)

    alias = scanner.expect(TokenType.IDENTIFIER).value;

    params = []
    if (t := scanner.peek()) and t.type is TokenType.LCHEVRON:
        params = analyze_type_params(ctx, scanner)

    scanner.expect(TokenType.EQ)

    target = analyze_type(ctx, scanner)

    return TypeAliasStatement(alias=alias, target=target, params=params)

def analyze_use(ctx: ParseCtx, scanner: TokenScanner) -> UseStatement:
    scanner.expect(TokenType.KW_USE)

    path = analyze_path(ctx, scanner)

    alias = None
    if scanner.match(TokenType.KW_AS):
        alias = scanner.expect(TokenType.IDENTIFIER).value

    return UseStatement(path=path, alias=alias)
        
def analyze_named_or_plain_value(ctx: ParseCtx, scanner: TokenScanner) -> AttributeValueNode:
    t = scanner.peek()
    if can_be_identifier(t):
        t2 = scanner.peek(1)
        if t2 and t2.type == TokenType.EQ:
            name = analyze_identifier(ctx, scanner)
            scanner.next()
            val = analyze_attribute_value(ctx, scanner)
            # Make sure named attribute values can also have multiple values.
            # This removes a level of nesting of attributes making it
            # easier to traverse the line
            return AttributeValueNode(val.value, name=name)
        return analyze_attribute_value(ctx, scanner)

    return analyze_attribute_value(ctx, scanner)


def analyze_attribute_composite_value(ctx: ParseCtx, scanner: TokenScanner, delimiter: TokenType) -> AttributeValueNode:
    end_delimiter: TokenType
    match delimiter:
        case TokenType.LPAREN:
            end_delimiter = TokenType.RPAREN
        case TokenType.LBRACKET:
            end_delimiter = TokenType.RBRACKET
        case TokenType.LBRACE:
            end_delimiter = TokenType.RBRACE
        case _:
            raise SyntaxError(f"Invalid attribute composite value delimiter '{delimiter}'")

    scanner.expect(delimiter)
    values = [analyze_named_or_plain_value(ctx, scanner)]

    while (t := scanner.peek()) and t.type == TokenType.COMMA:
        scanner.next()
        if (u := scanner.peek()) and u.type == end_delimiter:
            break
        res = analyze_named_or_plain_value(ctx, scanner)
        if values[-1].name is not None and res.name is None:
            raise SyntaxError("Cannot have an anonymous attribute value after a named value.", scanner.peek())
        values.append(res)

    scanner.expect(end_delimiter)

    return AttributeValueNode(values)
 
    
def analyze_attribute_value(ctx: ParseCtx, scanner: TokenScanner) -> AttributeValueNode:
    t = scanner.peek()
    cur_pos = scanner.pos
    closing_token_type = None
    match t.type:
        case TokenType.LPAREN | TokenType.LBRACKET | TokenType.LBRACE:
            try:
                return analyze_attribute_composite_value(ctx, scanner, t.type)
            except SyntaxError as e:
                print(e)
                scanner.backtrack(cur_pos)
                typ = analyze_type(ctx, scanner)
                return AttributeValueNode(typ)
        case _:
            typ = analyze_type(ctx, scanner)
            return AttributeValueNode(typ)

def analyze_attribute(ctx: ParseCtx, scanner: TokenScanner) -> AttributeNode:
    scanner.expect(TokenType.ATTR_BEGIN)

    id = analyze_identifier(ctx, scanner)
    if scanner.match(TokenType.RBRACKET):
        return AttributeNode(id)

    eq_sign = scanner.match(TokenType.EQ)

    res: AttributeValueNode = analyze_attribute_value(ctx, scanner)
    if not eq_sign and type(res.value) is not list:
        raise SyntaxError("Simple attribute values must be separated by an equal sign '=' from the attribute identifier.", scanner.peek())

    scanner.expect(TokenType.RBRACKET)

    return AttributeNode(id, value=res)


def analyze_attributes(ctx: ParseCtx, scanner: TokenScanner):
    list = []
    while (t := scanner.peek()) and t.type == TokenType.ATTR_BEGIN:
        list.append(analyze_attribute(ctx, scanner))
    return list

def analyze_struct_field(ctx: ParseCtx, scanner: TokenScanner) -> Optional[StructFieldNode]:
    key: Union[str, Identifier, McdocTypeNode] = None
    optional: bool = False
    attrs = analyze_attributes(ctx, scanner)
    key_tok = scanner.peek()
    match key_tok.type:
        case TokenType.STRING | TokenType.IDENTIFIER:
            scanner.next()
            key = Identifier(key_tok.value)
        case TokenType.LBRACKET:
            scanner.next()
            key = analyze_type(ctx, scanner)
            scanner.expect(TokenType.RBRACKET)
        case TokenType.KW_TYPE | TokenType.KW_USE | TokenType.KW_AS | TokenType.KW_INJECT |TokenType.KW_DISPATCH | TokenType.KW_TO:
            # Convert the keyword to an indentifier, as these ones are not reserved
            scanner.next()
            key = Identifier(key_tok.type.value)
        case TokenType.SPREAD:
            scanner.next()
            spread_type = analyze_type(ctx, scanner)
            return StructFieldNode(attributes=attrs, spread=True, type=spread_type)
        case _:
            raise SyntaxError(f"Invalid struct field (beginning with '{key_tok}')", key_tok)
    if scanner.match(TokenType.QUESTION):
        optional = True

    scanner.expect(TokenType.COLON)

    # attributes: List[Attribute] = []
    # if (t := scanner.peek()) and t.type == TokenType.ATTR_BEGIN:
    #     if not analyze_attributes(ctx, scanner, attributes) or not scanner.match(TokenType.SPREAD):
    #         return None

    type: McdocType = analyze_type(ctx, scanner)

    return StructFieldNode(attrs, type, key=key, optional=optional)

def analyze_struct(ctx: ParseCtx, scanner: TokenScanner):
    if not scanner.match(TokenType.KW_STRUCT):
        return False

    id: Identifier = None
    struct: Struct
    id_tok = scanner.peek()
    if can_be_identifier(id_tok):
        id = analyze_identifier(ctx, scanner)

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

def analyze_dispatch(ctx: ParseCtx, scanner: TokenScanner) -> DispatchStatement:
    scanner.expect(TokenType.KW_DISPATCH)

    source = scanner.expect(TokenType.RESID).value
    indices = analyze_type_indices(ctx, scanner)

    params = []
    if scanner.match_peek(TokenType.LCHEVRON):
        params = analyze_type_params(ctx, scanner)

    scanner.expect(TokenType.KW_TO)

    target = analyze_type(ctx, scanner)

    return DispatchStatement(source, target, source_indices=indices, type_params=params)

def analyze(ctx: ParseCtx):
    scanner: TokenScanner = TokenScanner(ctx.tokens)

    things = []
    pending_attrs = []
    while not scanner.at_end():
        t = scanner.peek()
        match t.type:
            case TokenType.KW_STRUCT:
                things.append(analyze_struct(ctx, scanner))
            case TokenType.KW_ENUM:
                things.append(analyze_enum(ctx, scanner))
            case TokenType.KW_USE:
                things.append(analyze_use(ctx, scanner))
            case TokenType.KW_TYPE:
                things.append(analyze_type_alias(ctx, scanner))
            case TokenType.KW_DISPATCH:
                things.append(analyze_dispatch(ctx, scanner))
            case TokenType.ATTR_BEGIN:
                pending_attrs.extend(analyze_attributes(ctx, scanner))
            case _:
                raise SyntaxError(f"Unexpected token '{t}'", t)

    return McdocFileNode(things)
