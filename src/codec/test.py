#! /usr/bin/env sh

import sys
from mcdoc_common import *
from mcdoc_ast import analyze
from mcdoc_ast_traversal import traverse
from mcdoc_binding import traverse_bind

def lex_number(ctx: ParseCtx, input: Scanner):
    decimal_dot: bool = False
    exponent: bool = False
    if input.prev_char() == '.':
        decimal_dot = True
    while (c := input.peek()) and c.isdigit():
        input.next_char()

    if c == '.':
        if (d := input.peek(1)) and d == '.':
            # Range !
            ctx.add_generic_token(TokenType.INTEGER, int(ctx.lexeme()))
            return
        decimal_dot = True
        input.next_char()


    while (c := input.peek()) and c.isdigit():
        input.next_char()

    if c == 'e' or c == 'E':
        exponent = True
        input.next_char()
        peeked = input.peek()
        if peeked == '-' or peeked == '+':
            input.next_char()

    while (c := input.peek()) and c.isdigit():
        input.next_char()

    string: str = ctx.lexeme()
    try:
        if decimal_dot or exponent:
            ctx.add_generic_token(TokenType.FLOAT, float(string))
        else:
            ctx.add_generic_token(TokenType.INTEGER, int(string))
    except ValueError:
        ctx.has_error = True
        print(f"Invalid float or integer '{string}'")
        
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

def is_valid_resid_path_char(c: str) -> bool:
    return c.isidentifier() or c.isdigit() or c == '/'

def lex_identifier(ctx: ParseCtx, input: Scanner):
    res_loc_delim_pos: int = -1
    while (c := input.peek()) and (c.isidentifier() or c.isdigit()):
        input.next_char()

    if input.peek() == ':':
        if is_valid_resid_path_char(input.peek(1)):
            res_loc_delim_pos = input.pos
            input.next_char()
            while (c := input.peek()) and is_valid_resid_path_char(c):
                input.next_char()

    text = ctx.lexeme()
    if res_loc_delim_pos >= 0:
        actual_delim_pos = res_loc_delim_pos - ctx.start
        ctx.add_generic_token(TokenType.RESID, ResourceID(namespace=text[:actual_delim_pos], path=text[actual_delim_pos + 1:]))
        return
                              
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
                e = input.peek(1)
                if e == '.':
                    ctx.add_token(TokenType.SPREAD)
                    input.next_char()
                else:
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

    print('')
    tree = analyze(ctx)
    traverse(tree)
    traverse_bind(tree)


parse(sys.argv[1])
