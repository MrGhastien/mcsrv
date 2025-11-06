#! /usr/bin/env sh

import sys
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
class Token:
    type: TokenType
    line: int
    column: int
    value: Optional[Union[str, int, float]] = None

    def __str__(self):
        """Pour print() et str()"""
        if self.value:
            return f"({self.type.name}: '{self.value}')"
        return f"{self.type.name}"
    
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

    def add_generic_token(self, tok_type: TokenType, value: Union[str, int, float]):
        self.tokens.append(Token(tok_type, self.input.line, self.input.column, value=value))

    def lexeme(self):
        return self.input.text[self.start:self.input.pos]

def lex_number(ctx: ParseCtx, input: Scanner):
    s = ""
    while (c := input.peek()) and c.isdigit():
        input.next_char()
        
def lex_string(ctx: ParseCtx, input: Scanner):
    string: str = ""
    while (c := input.peek()) is not None and c != '"':
        string += input.next_char()

    if not c:
        print("Unterminated string.")
        ctx.has_error = True
        return;

    input.next_char()

    ctx.add_generic_token(TokenType.STRING, string)

def lex_identifier(ctx: ParseCtx, input: Scanner):
    while (c := input.peek()) and (c.isidentifier() or c.isdigit()):
        input.next_char()

    text = ctx.lexeme()
    id_type: TokenType = keywords.get(text)
    if id_type is None:
        ctx.add_generic_token(TokenType.IDENTIFIER, text)
    else:
        ctx.add_token(id_type)

def lex_token(ctx: ParseCtx, input: Scanner):
    c = input.next_char()
    match c: 
        case '(': ctx.add_token(TokenType.LPAREN)
        case ')': ctx.add_token(TokenType.RPAREN)
        case '{': ctx.add_token(TokenType.LBRACE)
        case '}': ctx.add_token(TokenType.RBRACE)
        case '[': ctx.add_token(TokenType.LBRACKET)
        case ']': ctx.add_token(TokenType.RBRACKET)
        case '<': ctx.add_token(TokenType.LCHEVRON)
        case '>': ctx.add_token(TokenType.RCHEVRON)
        case ',': ctx.add_token(TokenType.COMMA)
        case '|': ctx.add_token(TokenType.PIPE)
        case '?': ctx.add_token(TokenType.QUESTION)
        case '@': ctx.add_token(TokenType.AT)
        case '=': ctx.add_token(TokenType.EQ)
        case '.':
            d = input.peek()
            if d == '.':
                ctx.add_token(TokenType.RANGE)
                input.next_char()
            elif d.isdigit():
                lex_number(ctx, input)
            else:
                ctx.add_token(TokenType.DOT)
        case ':':
            ctx.add_token(TokenType.DOUBLE_COLON if input.match(':') else TokenType.COLON)
        case '"': lex_string(ctx, input)
        case '/':
            if input.match('/'):
                while (d := input.peek()) and d != '\n':
                    input.next_char()
                    
            else:
                ctx.ha_error = True
                print(f"Unexpected character at {ctx.input.line}:{ctx.start}: {c}")
                
        case '#':
            if input.match('['):
                ctx.add_token(TokenType.ATTR_BEGIN)
            else:
                ctx.ha_error = True
                print(f"Unexpected character at {ctx.input.line}:{ctx.start}: {c}")
                
                
        case ' ' | '\t' | '\r' | '\n':
            return
        case _:
            if c.isidentifier() or c == '%':
                lex_identifier(ctx, input)
            elif c.isdigit() or c == '-' or c == '+':
                lex_number(ctx, input)
            else:
                ctx.ha_error = True
                print(f"Unexpected character at {ctx.input.line}:{ctx.start}: {c}")

def lex_tokens(ctx: ParseCtx, input: Scanner):
    
    while not input.at_end():
        ctx.start = input.pos
        lex_token(ctx, input)

def parse(filename: str):

    scanner = Scanner(filename)
    ctx = ParseCtx(scanner)
    lex_tokens(ctx, scanner)

    prev_line = 1
    for t in ctx.tokens:
        if t.line > prev_line:
            print()
            prev_line = t.line
        print(t, end=' ')


parse(sys.argv[1])
