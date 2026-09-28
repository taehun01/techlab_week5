#include "FFrustum.h"

FPlane::FPlane(const FVector& InPoint, const FVector& InNormal)
{
	Normal = InNormal / InNormal.Size();
	Distance = Normal.Dot(InPoint);
}

float FPlane::GetSignedDistanceToPlane(const FVector& Point) const
{
	return Normal.Dot(Point) - Distance;
}

bool FFrustum::Intersects(const FAxisAlignedBoundingBox& Box) const
{
	const FVector BoxCenter = Box.GetCenter();
	const FVector BoxExtents = Box.GetExtents();

	return BoxIsOnOrForwardPlane(BoxCenter, BoxExtents, Left) &&
		BoxIsOnOrForwardPlane(BoxCenter, BoxExtents, Right) &&
		BoxIsOnOrForwardPlane(BoxCenter, BoxExtents, Top) &&
		BoxIsOnOrForwardPlane(BoxCenter, BoxExtents, Bottom) &&
		BoxIsOnOrForwardPlane(BoxCenter, BoxExtents, Near) &&
		BoxIsOnOrForwardPlane(BoxCenter, BoxExtents, Far);
}

bool FFrustum::BoxIsOnOrForwardPlane(const FVector& BoxCenter, const FVector& BoxExtents, const FPlane& Plane) const
{
	const float R = BoxExtents.X * std::abs(Plane.Normal.X) +
		BoxExtents.Y * std::abs(Plane.Normal.Y) +
		BoxExtents.Z * std::abs(Plane.Normal.Z);

	return -R <= Plane.GetSignedDistanceToPlane(BoxCenter);
}