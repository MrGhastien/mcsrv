from mcdoc_binding import McdocFile
from mcdoc_types import *
from mcdoc_ast_nodes import McdocPath

from typing import List, Dict
from pathlib import Path

class BindingError(Exception):
    def __init__(self, message: str):
        super().__init__(message)

def mcdoc_path_to_fs(mcdoc_path: McdocPath, current_path: Path, root_path: Path) -> Path:
    fs_path: Path
    if mcdoc_path.absolute:
        fs_path = root_path
    else:
        fs_path = current_path
    for s in mcdoc_path.segments:
        if s == 'super':
            fs_path = fs_path.parent
        else:
            fs_path = fs_path / s
    fs_path = fs_path.resolve()
    if fs_path.is_dir():
        return fs_path / 'mod.mcdoc'
    
    return fs_path.with_suffix('.mcdoc').resolve()


class DependencyGraph:
    def __init__(self):
        self.graph = {}  # type_name -> [dependencies]

    def add_type(self, typ: McdocType):
        if self.is_visited(typ) or type(typ) is ParamType or typ is None:
            raise BindingError("Shit 2")
        self.graph[typ] = []

    def mark_dependency(self, typ: McdocType, dep: McdocType):
        if type(dep) is ParamType:
            print(f"Type parameter {typ.get_name()} cannot be a dependency")
            return
        if dep is None:
            raise BindingError("'None' depenency detected.")
        deps = self.graph[typ]
        if dep not in deps:
            deps.append(dep)

    def is_visited(self, typ: McdocType) -> bool:
        return typ in self.graph

    def sort_types(self) -> List[McdocType]:
        visited = set()
        res = []

        def visit(typ: McdocType):
            stack = [typ]
            while stack:
                t = stack.pop()

                if t in visited:
                    continue

                visited.add(t)

                for dep in reversed(self.graph[t]):
                    if dep == t:
                        print(f"Dependency cycle of {t.get_name()}")
                    elif dep not in visited:
                        visit(dep)

                match t:
                    case TypeAlias() as alias:
                        if len(alias.params) == 0:
                            res.append(t)
                    case DispatcherType():
                        pass
                    case _:
                        res.append(t)

        for t in self.graph.keys():
            visit(t)

        return res

    def print_dot(self):

        def safe_get_name(t: McdocType):
            tname = t.get_name()
            if type(t) is ListType:
                tname = f"List_{safe_get_name(t.elem_type)}"
            elif tname is None:
                tname = t.__name__
            return tname

        print('digraph types {')
        for k,v in self.graph.items():
            for t in v:
                print(f"  \"{k.get_unique_name()}\" -> \"{t.get_unique_name()}\";")
        print('}')

class FileRegistry:
    def __init__(self, root_path: Path):
        self.root_path = root_path
        self.files: Dict[Path, McdocFile] = {}
        self.dep_graph = DependencyGraph()
    
    def add_file(self, file: McdocFile):
        self.files[file.fs_path] = file
    
    def get_file(self, path: Path) -> McdocFile:
        return self.files.get(path)

    def revolve_type(self, file: McdocFile, use_path: McdocPath, locals: List[ParamType]) -> McdocType:
        current_path = file.fs_path
        if file.is_module:
            current_path = current_path.parent
        if len(use_path.segments) > 1:
            target_path = mcdoc_path_to_fs(use_path[:-1], current_path, self.root_path)
            used_file = self.get_file(target_path)
        else:
            # Search for local type parameters first.
            for l in locals:
                if l.get_name() == use_path[-1]:
                    return l
            used_file = file

        if used_file is None:
            raise BindingError(f"Unknown file {target_path} (tried to search {use_path} from {file.fs_path})")
        typ = used_file.find_type(use_path[-1])
        if typ is not None:
            return typ

        for u in file.uses:
            target_path = mcdoc_path_to_fs(u[:-1], current_path, self.root_path)
            #print(target_path)
            used_file = self.get_file(target_path)
            if used_file is None:
                raise BindingError(f"Could not find file '{target_path}'.")
            typ = used_file.find_type(use_path[-1])
            #`print(f"{u[-1]} -> {typ}")
            if typ is not None:
                break

        if typ is None:
            raise BindingError(f"Unbound reference to {use_path}.")
        return typ

    def get_all_types(self) -> List[McdocType]:
        res = []
        really_all_types = self.dep_graph.sort_types()

        def is_toplevel_type(typ: McdocType):
            for f in self.files.values():
                if typ in f.types:
                    return True
                elif typ in f.generic_instances:
                    return True
            return False

        for t in really_all_types:
            if is_toplevel_type(t):
                res.append(t)

        return res
            

def resolve_type_ref(registry: FileRegistry, file: McdocFile, type: McdocType, locals: List[ParamType]) -> McdocType:
    match type:
        case TypeRef() as ref:
            return registry.revolve_type(file, ref.target, locals)
        # case TypeAlias() as alias:
        #     assert len(locals) == 0
        #     resolve_ref(registry, file, alias.source, alias.params)
        #     return alias.source
        case StructType() | EnumType():
            if type.get_name() is not None and not locals: # Named inline definition !
                file.register_type(type)
            resolve_ref(registry, file, type, locals)
            return type
        case _:
            resolve_ref(registry, file, type, locals)
            return type

def resolve_ref(registry: FileRegistry, file: McdocFile, typ: McdocType, locals: List[ParamType]):
    if registry.dep_graph.is_visited(typ):
        return
    registry.dep_graph.add_type(typ)
    match typ:
        case StructType(fields=fields):
            for f in fields:
                if f.key_type is not None:
                    f.key_type = resolve_type_ref(registry, file, f.key_type, locals)
                    registry.dep_graph.mark_dependency(typ, f.key_type)
                f.typ = resolve_type_ref(registry, file, f.typ, locals)
                registry.dep_graph.mark_dependency(typ, f.typ)
        case UnionType() as union:
            for i in range(len(union.elements)):
                resolved = resolve_type_ref(registry, file, union.elements[i], locals)
                union.elements[i] = resolved
                registry.dep_graph.mark_dependency(typ, resolved)
        case TupleType() as tuple:
            for i in range(len(tuple.elements)):
                resolved = resolve_type_ref(registry, file, tuple.elements[i], locals)
                tuple.elements[i] = resolved
                registry.dep_graph.mark_dependency(typ, resolved)
        case ListType() as lst:
            resolved = resolve_type_ref(registry, file, lst.elem_type, locals)
            lst.elem_type = resolved
            registry.dep_graph.mark_dependency(typ, resolved)
        case TypeAlias() as alias:
            if len(locals) > 0:
                raise BindingError("Somehow, you managed to have multiple levels of type parameters. This should not be possible.")
            resolved = resolve_type_ref(registry, file, alias.source, alias.params)
            alias.source = resolved
            registry.dep_graph.mark_dependency(typ, resolved)
        case BuiltinType() | LiteralType():
            pass
        case EnumType() as enumm:
            registry.dep_graph.mark_dependency(typ, enumm.type)
            pass
        case DispatcherType() as dispatcher:
            print(f"TODO: {dispatcher}")
        case ArrayType() as arr:
            pass
        case ParamType():
            pass
        case GenericTypeInstance() as inst:
            print(inst.get_name())
            prev_name = inst.source.get_name()
            inst.source = resolve_type_ref(registry, file, inst.source, locals)
            assert inst.source.get_name() == prev_name
            for i in range(len(inst.args)):
                arg = inst.args[i]
                resolved = resolve_type_ref(registry, file, arg, locals)
                inst.args[i] = resolved
                registry.dep_graph.mark_dependency(inst, resolved)
            #print(inst.get_name())
            #print(inst)
            file.register_generic_instance(inst)
        case _:
            raise BindingError(f"[RESOLV] Not traversing {type(typ)}")
            #print(f"[RESOLV] Warning: Not traversing {type(typ)}")

def resolve_refs(registry: FileRegistry):
    for p, f in registry.files.items():
        print(f"Resolving {f.fs_path}...")
        for resid, ddict in f.dispatches.items():
            for idx, d in ddict.items():
                ddict[idx] = resolve_type_ref(registry, f, d, [])
            
        types_copy = f.types.copy()
        for t in types_copy:
            resolve_ref(registry, f, t, [])
        f.types = types_copy
