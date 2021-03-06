#pragma once

#include "Quat.h"

#include <limits>

class MeshObject;

// A plane is defined as the plane's normal and the distance of the plane to the origin
class PlaneCollider {
public:
	float d;
	Kore::vec3 normal;
};

class BoxCollider {
public:
	Kore::vec4 min;
	Kore::vec4 max;
    
    Kore::mat4 M;

	BoxCollider(Kore::vec4 min, Kore::vec4 max) : min(min), max(max) { }

	void trans(Kore::mat4 m) {
		Kore::vec4 mmin = m * min;
		Kore::vec4 mmax = m * max;

		min.x() = Kore::min(mmin.x(), mmax.x());
		max.x() = Kore::max(mmin.x(), mmax.x());

		min.y() = Kore::min(mmin.y(), mmax.y());
		max.y() = Kore::max(mmin.y(), mmax.y());

		min.z() = Kore::min(mmin.z(), mmax.z());
		max.z() = Kore::max(mmin.z(), mmax.z());
        M = m;
	}

	void swap(float &a, float &b) const {
		float t = a;
		a = b;
		b = t;
	}

	Kore::vec4 normalAt(const Kore::vec4 &point) const {
		Kore::vec4 size = (max - min);
		Kore::vec4 center = min + (size * 0.5f);
		Kore::vec4 normal;
		Kore::vec4 localPoint = point - center;
		float min = std::numeric_limits<float>::max();
		float distance = Kore::abs(size.x() - Kore::abs(localPoint.x()));
		if (distance < min) {
			min = distance;
			normal.set(1, 0, 0);
			normal *= localPoint.x() / Kore::abs(localPoint.x());
		}
		distance = Kore::abs(size.y() - Kore::abs(localPoint.y()));
		if (distance < min) {
			min = distance;
			normal.set(0, 1, 0);
			normal *= localPoint.y() / Kore::abs(localPoint.y());
		}
		distance = Kore::abs(size.z() - Kore::abs(localPoint.z()));
		if (distance < min) {
			min = distance;
			normal.set(0, 0, 1);
			normal *= localPoint.z() / Kore::abs(localPoint.z());
		}
		return normal;
	}

	bool IntersectsWith(Kore::vec3 orig, Kore::vec3 dir, float &dist, Kore::vec3 &norm) const {
		double tmin = -std::numeric_limits<double>::infinity();
		double tmax = std::numeric_limits<double>::infinity();
		
		if (dir.x() != 0.0) {
			double tx1 = (min.x() - orig.x()) / dir.x();
			double tx2 = (max.x() - orig.x()) / dir.x();

			tmin = Kore::max(tmin, Kore::min(tx1, tx2));
			tmax = Kore::min(tmax, Kore::max(tx1, tx2));
		}

		if (dir.y() != 0.0) {
			double ty1 = (min.y() - orig.y()) / dir.y();
			double ty2 = (max.y() - orig.y()) / dir.y();

			tmin = Kore::max(tmin, Kore::min(ty1, ty2));
			tmax = Kore::min(tmax, Kore::max(ty1, ty2));
		}

		if (dir.z() != 0.0) {
			double tz1 = (min.z() - orig.z()) / dir.z();
			double tz2 = (max.z() - orig.z()) / dir.z();

			tmin = Kore::max(tmin, Kore::min(tz1, tz2));
			tmax = Kore::min(tmax, Kore::max(tz1, tz2));
		}

		dist = tmin;

		Kore::vec3 v = orig + dir * dist;
		Kore::vec4 n = normalAt(Kore::vec4(v.x(), v.y(), v.z(), 1));
		norm.x() = n.x();
		norm.y() = n.y();
		norm.z() = n.z();
		return tmax >= 0 && tmax >= tmin;
	}

	bool IsInside(Kore::vec3 point) {
		return point.x() <= max.x() && point.x() >= min.x()
			&& point.y() <= max.y() && point.y() >= min.y()
			&& point.z() <= max.z() && point.z() >= min.z();
	}
};

class TriangleCollider {
	void LoadVector(Kore::vec3& v, int index, float* vb) {
		v.set(vb[index*8 + 0], vb[index*8 + 1], vb[index*8 + 2]);
	}

public:
	Kore::vec3 A;
	Kore::vec3 B;
	Kore::vec3 C;

	float Area() {
		return 0.5f * ((B-A).cross(C-A)).getLength();
	}

	void LoadFromBuffers(int index, int* ib, float* vb) {
		LoadVector(A, ib[index * 3], vb);
		LoadVector(B, ib[index * 3 + 1], vb);
		LoadVector(C, ib[index * 3 + 2], vb);
	}

	Kore::vec3 GetNormal() {
		Kore::vec3 n = (B-A).cross(C-A);
		n.normalize();
		return n;
	}

	PlaneCollider GetPlane() {
		Kore::vec3 n = GetNormal();
		float d = n.dot(A);

		PlaneCollider p;
		p.d = -d;
		p.normal = n;
		return p;
	}
};

class TriangleMeshCollider {
public:
	MeshObject* mesh;

	int lastCollision;
};

// A sphere is defined by a radius and a center.
class SphereCollider {
public:
	SphereCollider() {
	}

	Kore::vec3 center;
	float radius;

	// Return true iff there is an intersection with the other sphere
	bool IntersectsWith(const SphereCollider& other) {
		float distance = (other.center - center).getLength();
		return distance < other.radius + radius;
	}

	// Collision normal is the normal vector pointing towards the other sphere
	Kore::vec3 GetCollisionNormal(const SphereCollider& other) {
		Kore::vec3 n = other.center - center;
		n = n.normalize();
		return n;
	}

	// The penetration depth
	float PenetrationDepth(const SphereCollider& other) {
		return other.radius + radius - (other.center - center).getLength();
	}

	Kore::vec3 GetCollisionNormal(const PlaneCollider& other) {
		return other.normal;
	}

	float PenetrationDepth(const PlaneCollider &other) {
		return other.normal.dot(center) + other.d - radius;
	}

	float Distance(const PlaneCollider& other) {
		return other.normal.dot(center) + other.d;
	}

	bool IntersectsWith(const PlaneCollider& other) {
		return Kore::abs(Distance(other)) <= radius;
	}

	bool IsInside(const PlaneCollider& other) {
		return Distance(other) < radius;
	}

	bool IsOutside(const PlaneCollider& other) {
		return Distance(other) > radius;
	}

	/*
	bool IsInside(const BoxCollider& other) {
		if (!IsInside(other.posX))  { return false; }
		if (!IsInside(other.negX))  { return false; }
		if (!IsInside(other.posY))  { return false; }
		if (!IsInside(other.negY))  { return false; }
		if (!IsInside(other.posZ))  { return false; }
		if (!IsInside(other.negZ))  { return false; }

		return true;
	}

	bool IntersectsWithSides(const BoxCollider& other) {
		bool in_posX = !IsOutside(other.posX);
		bool in_negX = !IsOutside(other.negX);
		bool in_posY = !IsOutside(other.posY);
		bool in_negY = !IsOutside(other.negY);
		bool in_posZ = !IsOutside(other.posZ);
		bool in_negZ = !IsOutside(other.negZ);

		if (IntersectsWith(other.posZ) &&
			in_posX && in_negX && in_posY && in_negY) {
			return true;
		}

		if (IntersectsWith(other.negZ) &&
			in_posX && in_negX && in_posY && in_negY) {
			return true;
		}

		if (IntersectsWith(other.posX) &&
			in_posZ && in_negZ && in_posY && in_negY) {
			return true;
		}

		if (IntersectsWith(other.negX) &&
			in_posZ && in_negZ && in_posY && in_negY) {
			return true;
		}

		if (IntersectsWith(other.posY) &&
			in_posZ && in_negZ && in_posX && in_negX) {
			return true;
		}

		if (IntersectsWith(other.negY) &&
			in_posZ && in_negZ && in_posX && in_negX) {
			return true;
		}

		return false;
	}

	bool IntersectsWith(const BoxCollider& other) {
		return IsInside(other) || IntersectsWithSides(other);
	}
	*/
	Kore::vec3 GetCollisionNormal(const TriangleCollider& other) {
		// Use the normal of the triangle plane
		Kore::vec3 n = (other.B - other.A).cross(other.C - other.A);
		n.normalize();
		return n;
	}

	float PenetrationDepth(const TriangleCollider& other) {
		// Use the triangle plane
		// The assumption is that we have a mesh and will not collide only on a vertex or an edge

		return 0.0f;
	}

	bool IntersectsWith(TriangleMeshCollider& other);

	Kore::vec3 GetCollisionNormal(const TriangleMeshCollider& other);

	float PenetrationDepth(const TriangleMeshCollider& other);
	
	// Find the point where the sphere collided with the triangle mesh
	Kore::vec3 GetCollisionPoint(const TriangleMeshCollider& other) {
		// We take the point on the sphere that is closest to the triangle
		Kore::vec3 result = center - GetCollisionNormal(other) * radius;
		return result;
	}

	// Find a set of basis vectors such that the provided collision normal x is the first column of the basis matrix,
	// and the other two vectors are perpendicular to the collision normal
	Kore::mat3 GetCollisonBasis(Kore::vec3 x) {
		x.normalize();
		Kore::mat3 basis;
		basis.Set(0, 0, x.x());
		basis.Set(1, 0, x.y());
		basis.Set(2, 0, x.z());

		// Find a y-vector
		// x will often be the global y-axis, so don't use this -> use global z instead
		Kore::vec3 y(0, 0, 1);
		y = x.cross(y);
		y = y.normalize();

		basis.Set(0, 1, y.x());
		basis.Set(1, 1, y.y());
		basis.Set(2, 1, y.z());

		Kore::vec3 z = x.cross(y);
		z.normalize();

		basis.Set(0, 2, z.x());
		basis.Set(1, 2, z.y());
		basis.Set(2, 2, z.z());
		return basis;
	}

	bool IsSeparatedByVertexA(const TriangleCollider& other)
	{
		float distance = (other.A - center).getLength();
		bool distanceCheck = distance > radius;

		bool isSameDirectionB = (other.B - other.A).dot(other.A - center) > 0.0f;
		bool isSameDirectionC = (other.C - other.A).dot(other.A - center) > 0.0f;
		return distanceCheck & isSameDirectionB & isSameDirectionC;
	}

	bool IsSeparatedByVertexB(const TriangleCollider& other)
	{
		float distance = (other.B - center).getLength();
		bool distanceCheck = distance > radius;

		bool isSameDirectionA = (other.A - other.B).dot(other.B - center) > 0.0f;
		bool isSameDirectionC = (other.C - other.B).dot(other.B - center) > 0.0f;
		return distanceCheck & isSameDirectionA & isSameDirectionC;
	}

	bool IsSeparatedByVertexC(const TriangleCollider& other)
	{
		float distance = (other.C - center).getLength();
		bool distanceCheck = distance > radius;

		bool isSameDirectionA = (other.A - other.C).dot(other.C - center) > 0.0f;
		bool isSameDirectionB = (other.B - other.C).dot(other.C - center) > 0.0f;
		return distanceCheck & isSameDirectionA & isSameDirectionB;
	}

	bool IntersectsWith(const TriangleCollider& other) {
		// Separating Axes Test
		// 1 Face
		// 3 Vertices
		// 3 Edges
		// No overlap if all of the resulting axes are not touching

		Kore::vec3 A = other.A - center;
		Kore::vec3 B = other.B - center;
		Kore::vec3 C = other.C - center;
		float rr = radius * radius;
		Kore::vec3 V = (B - A).cross(C - A);
		float d = A.dot(V);
		float e = V.dot(V);
		bool sepPlane = d * d > rr * e;
		float aa = A.dot(A);
		float ab = A.dot(B);
		float ac = A.dot(C);
		float bb = B.dot(B);
		float bc = B.dot(C);
		float cc = C.dot(C);
		Kore::vec3 AB = B - A;
		Kore::vec3 BC = C - B;
		Kore::vec3 CA = A - C;
		float d1 = ab - aa;
		float d2 = bc - bb;
		float d3 = ac - cc;
		float e1 = AB.dot(AB);
		float e2 = BC.dot(BC);
		float e3 = CA.dot(CA);
		Kore::vec3 Q1 = A * e1 - d1 * AB;
		Kore::vec3 Q2 = B * e2 - d2 * BC;
		Kore::vec3 Q3 = C * e3 - d3 * CA;
		Kore::vec3 QC = C * e1 - Q1;
		Kore::vec3 QA = A * e2 - Q2;
		Kore::vec3 QB = B * e3 - Q3;
		bool sepEdge1 = (Q1.dot(Q1) > rr * e1 * e1) & (Q1.dot(QC) > 0);
		bool sepEdge2 = (Q2.dot(Q2) > rr * e2 * e2) & (Q2.dot(QA) > 0);
		bool sepEdge3 = (Q3.dot(Q3) > rr * e3 * e3) & (Q3.dot(QB) > 0);

		bool sepVertexA = IsSeparatedByVertexA(other);
		bool sepVertexB = IsSeparatedByVertexB(other);
		bool sepVertexC = IsSeparatedByVertexC(other);

		bool separated = sepPlane | sepVertexA | sepVertexB | sepVertexC | sepEdge1 | sepEdge2 | sepEdge3;
		
		return !separated;
	}

	// from the internetz, guaranteed to work
	bool IntersectsWith(Kore::vec3 orig, Kore::vec3 dir) const {
		float radius = this->radius; // size hack
		float t0, t1; // solutions for t if the ray intersects 
		// geometric solution
		Kore::vec3 L = center - orig;
		float tca = L * dir;
		// if (tca < 0) return false;
		float d2 = L * L - tca * tca;
		if (d2 > radius * radius) return false;
		float thc = Kore::sqrt(radius * radius - d2);
		t0 = tca - thc;
		t1 = tca + thc;

		if (t0 > t1) {
			float temp = t0;
			t0 = t1;
			t1 = temp;
		}

		if (t0 < 0) {
			t0 = t1; // if t0 is negative, let's use t1 instead 
			if (t0 < 0) return false; // both t0 and t1 are negative 
		}

		//t = t0;

		return true;
	}
};


