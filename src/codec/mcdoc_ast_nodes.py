from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import List, Tuple, Optional, Union

from mcdoc_common import *
from mcdoc_types import TypeKind, Range, TypeIndex

class ASTNode(ABC):
    pass

# Basic nodes
@dataclass
class Identifier:
    name: str

    def __str__(self) -> str:
        return self.name

@dataclass
class McdocPath:
    segments: List[str]
    absolute: bool = False

    def __str__(self) -> str:
        res = '::' if self.absolute else ''
        for s in self.segments[:-1]:
            res += f"{s}::"
        res += str(self.segments[-1])
        return res

    def __repr__(self) -> str:
        return self.__str__()

    def __getitem__(self, key):
        if isinstance(key, slice):
            return McdocPath(
                segments=self.segments[key],
                absolute=self.absolute
            )
        else:
            return self.segments[key]
    
    def __len__(self):
        return len(self.segments)
    
@dataclass
class AttributeValueNode(ASTNode):
    value: Union['McdocTypeNode', List['AttributeValue']]
    name: Optional[Identifier] = None

@dataclass
class AttributeNode(ASTNode):
    name: Identifier
    value: AttributeValueNode = None

# Type nodes
@dataclass
class UnattrTypeNode(ASTNode, ABC):
    kind: TypeKind

@dataclass
class McdocTypeNode(ASTNode):
    unattr_type: UnattrTypeNode
    attributes: List[AttributeNode] = None
    indices: List[TypeIndex] = None
    args: List['McdocTypeNode'] = None

    def __str__(self) -> str:
        return str(self.unattr_type)
    
@dataclass
class UnattrTypeRefNode(UnattrTypeNode):
    path: McdocPath

    def __str__(self) -> str:
        return str(self.path)            

@dataclass
class UnattrSimpleTypeNode(UnattrTypeNode):
    literal: Union[str, int, float, bool] = None
    value_range: Optional[Range] = None

    def __str__(self) -> str:
        return str(self.kind)

@dataclass
class UnattrArrayTypeNode(UnattrTypeNode):
    array_element_kind: TypeKind
    array_size_range: Optional[Range] = None
    value_range: Optional[Range] = None

@dataclass
class UnattrListTypeNode(UnattrTypeNode):
    elem_type: McdocTypeNode
    size_range: Optional[Range] = None

@dataclass
class UnattrCompositeTypeNode(UnattrTypeNode):
    elements: List[McdocTypeNode] = None

@dataclass
class UnattrDispatcherTypeNode(UnattrTypeNode):
    source: ResourceID
    indices: List[TypeIndex]

@dataclass
class UnattrEnumTypeNode(UnattrTypeNode):
    name: Identifier
    value_kind: TypeKind
    fields: List[Tuple[Identifier, Union[int, float, str]]]

@dataclass
class StructFieldNode:
    attributes: List[AttributeNode]
    type: McdocTypeNode
    key: Union[Identifier, McdocTypeNode] = None
    optional: bool = False
    spread: bool = False

@dataclass
class UnattrStructTypeNode(UnattrTypeNode):
    fields: List[StructFieldNode]
    name: Identifier

    def __str__(self) -> str:
        res = "struct"
        if self.name:
            res += f" {self.name}"
        res += '{'

        for f in self.fields:
            res += str(f)
        res += '}'

        return res
    

@dataclass
class McdocFileNode(ASTNode):
    things: List[ASTNode]

@dataclass
class UseStatement(ASTNode):
    path: McdocPath
    alias: Optional[Identifier] = None

@dataclass
class TypeAliasStatement(ASTNode):
    alias: Identifier
    target: McdocTypeNode
    params: List[Identifier] = None

@dataclass
class DispatchStatement(ASTNode):
    source: ResourceID
    target: McdocTypeNode
    source_indices: List[TypeIndex]
    type_params: List[Identifier] = None
