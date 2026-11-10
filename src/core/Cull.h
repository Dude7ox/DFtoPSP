#pragma once
#include "SoftwareRenderer.h"
namespace dfpsp {
// Six inward-facing planes (n.x*x+n.y*y+n.z*z+d >= 0 is inside).
struct Frustum { float p[6][4]; };
void frustumFromCamera(const Camera& cam,float aspect,float nearZ,float farZ,Frustum& out);
// Rejects a triangle only when all three verts are behind the same plane.
bool triInView(const Tri& t,const Frustum& f);
// Sphere test for billboards/flats.
bool sphereInView(const Vec3& c,float r,const Frustum& f);
} // namespace
