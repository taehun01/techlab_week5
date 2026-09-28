#include "FOctree.h"

namespace
{
	bool IsInLooseBounds(const FAxisAlignedBoundingBox& Bounds, const FVector& Center, float HalfSize, float LooseFactor)
	{
		const float Loose = HalfSize * LooseFactor;
		return Bounds.Min.X >= Center.X - Loose && Bounds.Max.X <= Center.X + Loose
			&& Bounds.Min.Y >= Center.Y - Loose && Bounds.Max.Y <= Center.Y + Loose
			&& Bounds.Min.Z >= Center.Z - Loose && Bounds.Max.Z <= Center.Z + Loose;
	}

	int32 ChildOctant(const FAxisAlignedBoundingBox& Bounds, const FVector& Center)
	{
		const FVector ElementCenter = (Bounds.Min + Bounds.Max) * 0.5f;
		return (ElementCenter.X >= Center.X ? 4 : 0)
			| (ElementCenter.Y >= Center.Y ? 2 : 0)
			| (ElementCenter.Z >= Center.Z ? 1 : 0);
	}

	bool IsIntersect(const FAxisAlignedBoundingBox& A, const FAxisAlignedBoundingBox& B)
	{
		return !(A.Min.X > B.Max.X
			|| A.Min.Y > B.Max.Y
			|| A.Min.Z > B.Max.Z
			|| A.Max.X < B.Min.X
			|| A.Max.Y < B.Min.Y
			|| A.Max.Z < B.Min.Z);
	}

	FAxisAlignedBoundingBox ToAABB(const FVector& Center, float HalfSize)
	{
		FVector Half = FVector(HalfSize, HalfSize, HalfSize);

		FAxisAlignedBoundingBox Box;
		Box.Min = Center - Half;
		Box.Max = Center + Half;

		return Box;
	}
}

FOctree::FOctree(const FVector& RootCenter, float RootHalfSize, const FOctreeSettings& InSettings)
	: Settings(InSettings)
{
	FNode Root;
	Root.Center = RootCenter;
	Root.HalfSize = RootHalfSize;
	Root.FirstChild = -1;
	Root.Depth = 0;

	Nodes.push_back(Root);
	Elements.resize(1);
}

void FOctree::Insert(const FOctreeElement& Element)
{
	int32 NodeIndex = 0;

	while (true)
	{
		FNode& Node = Nodes[NodeIndex];

		// 리프 노드가 아닌 경우
		if (Node.FirstChild != -1)
		{
			int32 ChildIndex = Node.FirstChild + ChildOctant(Element.Bounds, Node.Center);
			if (IsInLooseBounds(Element.Bounds, Nodes[ChildIndex].Center, Nodes[ChildIndex].HalfSize, Settings.LooseFactor))
			{
				NodeIndex = ChildIndex;
				continue;
			}
			AddToNode(NodeIndex, Element);
			return;
		}

		// 현재 노드에 자리 있음 || 최대 깊이
		if (static_cast<int32>(Elements[NodeIndex].size()) < Settings.MaxElementsPerNode || Node.Depth >= Settings.MaxDepth)
		{
			AddToNode(NodeIndex, Element);
			return;
		}

		Split(NodeIndex);
	}
}

void FOctree::Remove(int32 ObjectIndex)
{
	auto It = ElementIdByObjectIndex.find(ObjectIndex);
	if (It == ElementIdByObjectIndex.end())
	{
		return;
	}

	auto [NodeIndex, SlotIndex] = It->second;
	ElementIdByObjectIndex.erase(It);

	TArray<FOctreeElement>& NodeElements = Elements[NodeIndex];
	int32 LastIndex = static_cast<int32>(NodeElements.size()) - 1;

	if (SlotIndex != LastIndex)
	{
		NodeElements[SlotIndex] = NodeElements[LastIndex];
		ElementIdByObjectIndex[NodeElements[SlotIndex].ObjectIndex].SlotIndex = SlotIndex;
	}
	NodeElements.pop_back();
}

void FOctree::Update(int32 ObjectIndex, const FAxisAlignedBoundingBox& NewBounds)
{
	// TODO: 소속 Node가 변경되지 않는다면 Bounds만 교체하는 방법 추가
	Remove(ObjectIndex);
	Insert({ ObjectIndex, NewBounds });
}

void FOctree::Clear()
{
	FNode Root;
	Root.Center = Nodes[0].Center;
	Root.HalfSize = Nodes[0].HalfSize;
	Root.FirstChild = -1;
	Root.Depth = 0;

	Nodes.clear();
	FreeNodeBlocks.clear();
	Elements.clear();
	ElementIdByObjectIndex.clear();

	Nodes.push_back(Root);
	Elements.resize(1);
}

void FOctree::QueryAABB(const FAxisAlignedBoundingBox& Box, TArray<int32>& OutObjects) const
{
	QueryAABB(Box, OutObjects, 0);
}

void FOctree::AddToNode(int32 NodeIndex, const FOctreeElement& Element)
{
	ElementIdByObjectIndex[Element.ObjectIndex]
		= { NodeIndex, static_cast<int32>(Elements[NodeIndex].size()) };
	Elements[NodeIndex].push_back(Element);
}

void FOctree::Split(int32 NodeIndex)
{
	// 재사용 노드가 있는 경우
	if (!FreeNodeBlocks.empty())
	{
		Nodes[NodeIndex].FirstChild = FreeNodeBlocks.back();
		FreeNodeBlocks.pop_back();
	}
	else
	{
		Nodes[NodeIndex].FirstChild = static_cast<int32>(Nodes.size());
		Nodes.resize(Nodes.size() + 8);
		Elements.resize(Nodes.size());
	}

	FNode& Node = Nodes[NodeIndex];
	float Half = Nodes[NodeIndex].HalfSize * 0.5f;

	for (int32 I = 0; I < 8; I++)
	{
		float X = (I & 4 ? 1.0f : -1.0f) * Half;
		float Y = (I & 2 ? 1.0f : -1.0f) * Half;
		float Z = (I & 1 ? 1.0f : -1.0f) * Half;

		int32 ChildIndex = Node.FirstChild + I;
		Nodes[ChildIndex].Center = Node.Center + FVector(X, Y, Z);
		Nodes[ChildIndex].HalfSize = Half;
		Nodes[ChildIndex].FirstChild = -1;
		Nodes[ChildIndex].Depth = Node.Depth + 1;
	}

	TArray<FOctreeElement> Temp;
	for (const FOctreeElement& Element : Elements[NodeIndex])
	{
		int32 ChildIndex = Node.FirstChild + ChildOctant(Element.Bounds, Node.Center);
		FNode& Child = Nodes[ChildIndex];

		if (IsInLooseBounds(Element.Bounds, Child.Center, Child.HalfSize, Settings.LooseFactor))
		{
			AddToNode(ChildIndex, Element);
		}
		else
		{
			Temp.push_back(Element);
		}
	}

	// TODO: 나중에 더 효율적인 방식으로 변경
	Elements[NodeIndex].clear();
	for (const FOctreeElement& Element : Temp)
	{
		AddToNode(NodeIndex, Element);
	}
}

void FOctree::QueryAABB(const FAxisAlignedBoundingBox& Box, TArray<int32>& OutObjects, int32 NodeIndex) const
{
	for (const auto& Element : Elements[NodeIndex])
	{
		if (IsIntersect(Element.Bounds, Box))
		{
			OutObjects.push_back(Element.ObjectIndex);
		}
	}

	if (Nodes[NodeIndex].FirstChild == -1)
	{
		return;
	}

	for (int32 I = 0; I < 8; I++)
	{
		int32 ChildIndex = Nodes[NodeIndex].FirstChild + I;
		const FNode& Node = Nodes[ChildIndex];

		if (IsIntersect(ToAABB(Node.Center, Node.HalfSize * Settings.LooseFactor), Box))
		{
			QueryAABB(Box, OutObjects, ChildIndex);
		}
	}
}
