from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import List, Tuple, Optional, Union

from mcdoc_common import *
from mcdoc_types import TypeKind, Range

class ASTNode(ABC):
    pass

# Basic nodes
@dataclass
class Identifier:
    name: str

    def __str__(self) -> str:
        return self.name

@dataclass
class Path:
    segments: List[Union[Identifier, TokenType]]
    absolute: bool = False

    def __str__(self) -> str:
        res = '::' if self.absolute else ''
        for s in self.segments[:-1]:
            res += f"{s}::"
        res += str(self.segments[-1])
        return res

    def __repr__(self) -> str:
        return self.__str__()
    
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
class TypeIndex:
    key: List
    dynamic: bool = False

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
    path: Path

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
            res += str(field)
        res += '}'
    

@dataclass
class McdocFile(ASTNode):
    things: List[ASTNode]

@dataclass
class UseStatement(ASTNode):
    path: Path
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
