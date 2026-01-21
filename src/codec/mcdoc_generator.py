from typing import List, Dict
from mcdoc_types import *
from mcdoc_resolv import FileRegistry
from pathlib import Path


class GeneratorError(Exception):
    def __init__(self, message: str, node=None):
        super().__init__(message)

# struct
# json -> struct
# struct -> nbt
# nbt -> struct
class CCodeWriter:
    """Générateur de code C avec gestion de l'indentation"""
    
    def __init__(self, indent_str: str = "    "):
        self.lines: List[str] = []
        self.indent_level: int = 0
        self.indent_str = indent_str
        self._cur_line = ""
    
    def write(self, line: str = "", newline: bool = True):
        """Écrit une ligne avec indentation"""
        if line:
            indent = self.indent_str * self.indent_level if not self._cur_line else ""
            self._cur_line += indent + line

        if newline:
            self.lines.append(self._cur_line)
            self._cur_line = ""
    
    def writeln(self, *lines: str):
        """Écrit plusieurs lignes"""
        for line in lines:
            self.write(line)
    
    def indent(self):
        """Augmente l'indentation"""
        self.indent_level += 1
    
    def dedent(self):
        """Diminue l'indentation"""
        self.indent_level = max(0, self.indent_level - 1)
    
    def block_start(self, header: str):
        """Commence un bloc"""
        self.write(header + " {")
        self.indent()
    
    def block_end(self, suffix: str = "", newline: bool = True):
        """Termine un bloc"""
        self.dedent()
        self.write("}" + suffix, newline=newline)
    
    def include(self, header: str, system: bool = True):
        """Ajoute un #include"""
        if system:
            self.write(f"#include <{header}>")
        else:
            self.write(f'#include "{header}"')
    
    def header_guard(self, name: str):
        """Génère un header guard"""
        guard = name.upper().replace(".", "_")
        self.write(f"#ifndef {guard}")
        self.write(f"#define {guard}")
        self.write()
    
    def header_guard_end(self, name: str):
        """Termine un header guard"""
        guard = name.upper().replace(".", "_")
        self.write()
        self.write(f"#endif /* ! {guard} */")
    
    def comment(self, text: str):
        """Ajoute un commentaire"""
        self.write(f"// {text}")
    
    def block_comment(self, *lines: str):
        """Ajoute un commentaire multi-lignes"""
        self.write("/*")
        for line in lines:
            self.write(f" * {line}")
        self.write(" */")
    
    def typedef_struct(self, name: str):
        """Forward declaration d'une struct"""
        self.write(f"typedef struct {name} {name};")
    
    def get_code(self) -> str:
        """Retourne tout le code"""
        return "\n".join(self.lines)
    
    def save(self, path: Path):
        """Sauvegarde dans un fichier"""
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(self.get_code(), encoding='utf-8')

def prevent_keyword_collision(name: str):
    match name:
        case 'default':
            return 'dfault'
        case _:
            return name

def generate_enum(enumm: EnumType, writer: CCodeWriter):

    def generate_simple_enum(enumm: EnumType, writer: CCodeWriter):
        for field in enumm.fields:
            writer.write(f"{enumm.name.upper()}_{field.name.upper()},")
        writer.block_end(';')

    enum_name = prevent_keyword_collision(enumm.name)
    writer.block_start(f"enum {enum_name}")

    if enumm.type.type == TypeKind.FLOAT:
        generate_simple_enum(enumm, writer)
        writer.block_start(f"static const float {enum_name}_enum_values[] = ")
        for field in enumm.fields:
            writer.write(f"[{enumm.name.upper()}_{field.name.upper()}] = {field.value},")
        writer.block_end(newline=False)
    elif enumm.type.type == TypeKind.STRING:
        generate_simple_enum(enumm, writer)
        writer.block_start(f"static const char* {enum_name}_enum_values[] = ")
        for field in enumm.fields:
            writer.write(f"[{enumm.name.upper()}_{field.name.upper()}] = \"{field.value}\",")
        writer.block_end(newline=False)
    else:
        for field in enumm.fields:
            writer.write(f"{enumm.name.upper()}_{field.name.upper()} = {field.value},")
        writer.block_end(newline=False)

def generate_struct(struct: StructType, writer: CCodeWriter, type_param_bindings: Dict[ParamType, McdocType]):
    struct_name = prevent_keyword_collision(struct.get_name())
    if struct_name is None:
        writer.block_start(f"struct")
    else:
        writer.block_start(f"struct {struct_name}")
    for field in struct.fields:
        fname = prevent_keyword_collision(field.name)
        if field.optional:
            writer.write(f"bool has_{fname};")
        if field.key_type is not None:
            key_type = field.key_type
            if type(key_type) is ParamType:
                key_type = type_param_bindings[key_type.get_name()]
            writer.write(f"Dict dynamic_keys; /* [", newline=False)
            generate_codec_single(field.key_type, writer, type_param_bindings)
            writer.write(']: ')
            generate_codec_single(field.typ, writer, type_param_bindings)
            writer.write(' */')
        else:
            ftype = field.typ
            if type(ftype) is ParamType:
                ftype = type_param_bindings[ftype.get_name()]

            if field.spread:
                generate_codec_single(ftype, writer, type_param_bindings)
                writer.write(f" super;")
                continue

            if type(ftype) is ListType:
                generate_codec_single(ftype.elem_type, writer, type_param_bindings)
                writer.write(f"* {fname};")
                writer.write(f"u32 {fname}_count;")
            else:
                generate_codec_single(ftype, writer, type_param_bindings)
                writer.write(f" {fname};")
    writer.block_end(newline=False)

union_names = [c for c in 'abcdefghijklmnopqrstuvwxyz']

def generate_codec_single(typ: McdocType, writer: CCodeWriter, type_param_bindings: Dict[ParamType, McdocType]):
    match typ:
        case StructType() as struct:
            name = struct.get_name()
            if name is None:
                generate_struct(struct, writer, type_param_bindings)
            else:
                writer.write(f"struct {name}", newline=False)
        case EnumType() as enumm:
            name = enumm.get_name()
            if name is None:
                generate_enum(enumm, writer)
            writer.write(f"enum {enumm.name}", newline=False)
        case UnionType() as union:
            writer.block_start('union')
            i = 0
            for e in union.elements:
                if type(e) is ParamType:
                    e = type_param_bindings[e.get_name()]
                generate_codec_single(e, writer, type_param_bindings)
                writer.write(f" {union_names[i]};")
                i += 1
            writer.block_end(newline=False)
        case BuiltinType() as builtin:
            writer.write(builtin.get_c_name(), newline=False)
        case ListType() as lst:
            writer.write(f"struct List_{lst.elem_type.get_name()}", newline=False)
        case TupleType() as tpl:
            name = "Tuple"
            for e in tpl.elements:
                name += f"_{e.get_name()}"
            writer.write(f"struct {name}", newline=False)
        case GenericTypeInstance() as inst:
            writer.write(typ.get_c_name(), newline=False)
        case DispatcherType():
            pass
        case ArrayType() as arr:
            pass
        case LiteralType() as literal:
            writer.write(literal.get_c_name(), newline=False)
            writer.write(f" /* Literal: {literal.value} */", newline=False)
        case TypeAlias() as alias:
            writer.write(alias.get_name(), newline=False)
        # Handle:
        # - Type dispatching (static with lookup in python code, dynamic with union of structs)
        # - Spreading fields (Composition of struct)
        # - Optionals with a dedicated type (and macros)
        # - Arrays (not lists!)
        # - Better names for union and tuple member fields
        case _:
            raise GeneratorError(f"Unhandled type codec for {typ} is {type(typ)}")


def alias_name_with_bindings(alias: TypeAlias, type_param_bindings: Dict[str, McdocType]) -> str:
    res = alias.get_name()
    for p in alias.params:
        arg = type_param_bindings[p.get_name()]
        res += f"_{arg.get_name()}"
    return res
        
def generate_codec_single_top_level(typ: McdocType, writer: CCodeWriter, type_param_bindings: Dict[str, McdocType]):
    match typ:
        case StructType() as struct:
            generate_struct(struct, writer, type_param_bindings)
            writer.write(';')
        case EnumType() as enumm:
            generate_enum(enumm, writer)
            writer.write(';')
        case TypeAlias() as alias:
            if len(alias.params) == len(type_param_bindings):
                writer.write(f"typedef ", newline=False)
                generate_codec_single(alias.source, writer, type_param_bindings)
                writer.write(f" {alias_name_with_bindings(alias, type_param_bindings)};")
        case GenericTypeInstance() as gen:
            new_bindings = {}
            print(gen.get_name())
            print(gen.args)
            print(gen.source.params)
            for i in range(len(gen.args)):
                arg = gen.args[i]
                param = gen.source.params[i]
                new_bindings[param.get_name()] = arg

            generate_codec_single_top_level(gen.source, writer, new_bindings)
        # case BuiltinType() | LiteralType() | ListType() | ArrayType() | UnionType() | TupleType():
        #     pass
        case _:
            raise GeneratorError(f"Unhandled type top level codec for {typ.get_name()} is {type(typ)}")

    writer.write()

def generate_codecs(registry: FileRegistry):
    writer = CCodeWriter()
    writer.include('definitions.h', system=False)
    writer.include('utils/string.h', system=False)
    writer.write()
    writer.block_comment('This code has been generated by a python script, from mcdoc definitions.', 'Do not modify this file directly.')
    writer.write()

    registry.dep_graph.print_dot()

    for t in registry.get_all_types():
        generate_codec_single_top_level(t, writer, {})
    writer.save(Path('out_codec.c'))
