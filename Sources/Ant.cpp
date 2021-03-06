#include "pch.h"
#include "Ant.h"
#include "Engine/InstancedMeshObject.h"
#include "Engine/TriggerCollider.h"
#include "KitchenObject.h"

#include <assert.h>
#include <Kore/Math/Random.h>

using namespace Kore;

namespace {
	Kore::VertexBuffer** vertexBuffers;
	InstancedMeshObject* body;
	InstancedMeshObject* leg;

	const int maxAnts = 500;
	Ant ants[maxAnts];
	float* scent;
	const int scents = 100;
    int antsDead = 0;
	
	float scentAt(int x, int y, int z) {
		if (z >= scents || y >= scents || z >= scents) return 0;
		if (z < 0 || y < 0 || x < 0) return 0;
		return scent[z * scents * scents + y * scents + x];
	}

	void setScent(int x, int y, int z, float value) {
		if (z >= scents || y >= scents || z >= scents) return;
		if (z < 0 || y < 0 || x < 0) return;
		scent[z * scents * scents + y * scents + x] = value;
	}

	int gridPosition(float pos) {
		return Kore::round(pos + scents / 2);
	}

	vec3i gridPosition(vec3 pos) {
		vec3i gridPos;
		gridPos.x() = gridPosition(pos.x());
		gridPos.y() = gridPosition(pos.y());
		gridPos.z() = gridPosition(pos.z());
		return gridPos;
	}

	float realPosition(int grid) {
		return grid - scents / 2;
	}

	vec3 realPosition(vec3i grid) {
		vec3 pos;
		pos.x() = realPosition(grid.x());
		pos.y() = realPosition(grid.y());
		pos.z() = realPosition(grid.z());
		return pos;
	}

	int count = 0;
}

Ant::Ant() : mode(Floor) {
	rotation = mat4::Identity();
	forward = vec4(0, 0, -1, 0);
	right = vec4(1, 0, 0, 0);
	up = vec4(0, 1, 0, 0);
}

void Ant::init() {
	scent = new float[scents * scents * scents];
	for (int i = 0; i < scents * scents * scents; ++i) {
		scent[i] = Random::get(100) / 200.0f;
	}

	VertexStructure** structures = new VertexStructure*[2];
	structures[0] = new VertexStructure();
	structures[0]->add("pos", Float3VertexData);
	structures[0]->add("tex", Float2VertexData);
	structures[0]->add("nor", Float3VertexData);

	structures[1] = new VertexStructure();
	structures[1]->add("M", Float4x4VertexData);
	structures[1]->add("N", Float4x4VertexData);
	structures[1]->add("tint", Float4VertexData);

	body = new InstancedMeshObject("Data/Meshes/ant_body.obj", "Data/Textures/tank_bottom.png", structures, 10, 10);
	leg = new InstancedMeshObject("Data/Meshes/ant_leg.obj", "Data/Textures/tank_bottom.png", structures, 10, 10);

	vertexBuffers = new VertexBuffer*[2];
	vertexBuffers[0] = body->vertexBuffers[0];
	vertexBuffers[1] = new VertexBuffer(maxAnts, *structures[1], 1);

	for (int i = 0; i < maxAnts; ++i) {
		vec3 start(0, 1.5, 0);
		ants[i].position = vec3(start.x() + Random::get(-100, 100) / 100.0f, start.y(), start.z() + Random::get(-100, 100) / 100.0f); // vec3(Random::get(-100, 100) / 10.0f, -1, Random::get(-100, 100) / 10.0f);
		//ants[i].rotation = Quaternion(ants[i].right, Random::get(3000.0f) / 1000.0f).matrix() * ants[i].rotation;
        
        ants[i].energy = 0;
        ants[i].dead = false;
	}
}

void Ant::chooseScent(bool force) {
	vec3i grid = gridPosition(position);
	if (force || grid != lastGrid) {
		if (!force) {
			float scent = scentAt(grid.x(), grid.y(), grid.z());
			scent = Kore::min(scent + 0.2f, 1.0f);
			setScent(grid.x(), grid.y(), grid.z(), scent);
		}
		lastGrid = grid;
		vec3i nextGrid = gridPosition(position + vec3(forward.x(), forward.y(), forward.z()) * 1.0f);
		if (mode == Floor) {
			vec3i neighbours[8];
			for (int i = 0; i < 8; ++i) neighbours[i] = grid;
			neighbours[0].x() -= 1;
			neighbours[0].z() += 1;
			neighbours[1].x() += 0;
			neighbours[1].z() += 1;
			neighbours[2].x() += 1;
			neighbours[2].z() += 1;
			neighbours[3].x() += 1;
			neighbours[3].z() += 0;
			neighbours[4].x() += 1;
			neighbours[4].z() -= 1;
			neighbours[5].x() += 0;
			neighbours[5].z() -= 1;
			neighbours[6].x() -= 1;
			neighbours[6].z() -= 1;
			neighbours[7].x() -= 1;
			neighbours[7].z() += 0;

			float maxScent = 0;
			for (int i = 0; i < 8; ++i) {
				float scent = scentAt(neighbours[i].x(), neighbours[i].y(), neighbours[i].z());
				if (scent > maxScent) {
					int i1 = i - 1 < 0 ? 7 : i - 1;
					int i2 = i;
					int i3 = i + 1 > 7 ? 0 : i + 1;
					if (nextGrid == neighbours[i1] || nextGrid == neighbours[i2] || nextGrid == neighbours[i3]) {
						maxScent = scent;
						vec3 pos = realPosition(neighbours[i]);
						vec3 forward = pos - position;
						forward = forward.normalize();
						//vec3 right = forward.cross(vec3(up.x(), up.y(), up.z()));
						vec3 right = vec3(up.x(), up.y(), up.z()).cross(forward);
						this->right = vec4(right.x(), right.y(), right.z(), 0);
						this->forward = vec4(forward.x(), forward.y(), forward.z(), 0);
						float angle = Kore::atan2(forward.z(), forward.x());
						rotation = Quaternion(vec3(up.x(), up.y(), up.z()), angle + pi / 2.0f).matrix();
					}
				}
			}
		}
		else if (mode == FrontWall) {
			vec3i neighbours[8];
			for (int i = 0; i < 8; ++i) neighbours[i] = grid;
			neighbours[0].x() -= 1;
			neighbours[0].y() += 1;
			neighbours[1].x() += 0;
			neighbours[1].y() += 1;
			neighbours[2].x() += 1;
			neighbours[2].y() += 1;
			neighbours[3].x() += 1;
			neighbours[3].y() += 0;
			neighbours[4].x() += 1;
			neighbours[4].y() -= 1;
			neighbours[5].x() += 0;
			neighbours[5].y() -= 1;
			neighbours[6].x() -= 1;
			neighbours[6].y() -= 1;
			neighbours[7].x() -= 1;
			neighbours[7].y() += 0;

			float maxScent = 0;
			for (int i = 0; i < 8; ++i) {
				float scent = scentAt(neighbours[i].x(), neighbours[i].y(), neighbours[i].z());
				if (scent > maxScent) {
					int i1 = i - 1 < 0 ? 7 : i - 1;
					int i2 = i;
					int i3 = i + 1 > 7 ? 0 : i + 1;
					if (nextGrid == neighbours[i1] || nextGrid == neighbours[i2] || nextGrid == neighbours[i3]) {
						maxScent = scent;
						vec3 pos = realPosition(neighbours[i]);
						vec3 forward = pos - position;
						forward = forward.normalize();
						up = vec4(0, 0, -1, 0);
						vec3 right = forward.cross(vec3(up.x(), up.y(), up.z()));
						//vec3 right = vec3(up.x(), up.y(), up.z()).cross(forward);
						this->right = vec4(right.x(), right.y(), right.z(), 0);
						this->forward = vec4(forward.x(), forward.y(), forward.z(), 0);
						float angle = Kore::atan2(forward.y(), forward.x());
						rotation = Quaternion(vec3(up.x(), up.y(), up.z()), angle + pi / 2.0f).matrix() * Quaternion(vec3(1, 0, 0), pi / 2.0f).matrix();
					}
				}
			}
		}
		else if (mode == BackWall) {
			vec3i neighbours[8];
			for (int i = 0; i < 8; ++i) neighbours[i] = grid;
			neighbours[0].x() -= 1;
			neighbours[0].y() += 1;
			neighbours[1].x() += 0;
			neighbours[1].y() += 1;
			neighbours[2].x() += 1;
			neighbours[2].y() += 1;
			neighbours[3].x() += 1;
			neighbours[3].y() += 0;
			neighbours[4].x() += 1;
			neighbours[4].y() -= 1;
			neighbours[5].x() += 0;
			neighbours[5].y() -= 1;
			neighbours[6].x() -= 1;
			neighbours[6].y() -= 1;
			neighbours[7].x() -= 1;
			neighbours[7].y() += 0;

			float maxScent = 0;
			for (int i = 0; i < 8; ++i) {
				float scent = scentAt(neighbours[i].x(), neighbours[i].y(), neighbours[i].z());
				if (scent > maxScent) {
					int i1 = i - 1 < 0 ? 7 : i - 1;
					int i2 = i;
					int i3 = i + 1 > 7 ? 0 : i + 1;
					if (nextGrid == neighbours[i1] || nextGrid == neighbours[i2] || nextGrid == neighbours[i3]) {
						maxScent = scent;
						vec3 pos = realPosition(neighbours[i]);
						vec3 forward = pos - position;
						forward = forward.normalize();
						up = vec4(0, 0, -1, 0);
						vec3 right = forward.cross(vec3(up.x(), up.y(), up.z()));
						//vec3 right = vec3(up.x(), up.y(), up.z()).cross(forward);
						this->right = vec4(right.x(), right.y(), right.z(), 0);
						this->forward = vec4(forward.x(), forward.y(), forward.z(), 0);
						float angle = Kore::atan2(forward.y(), forward.x());
						rotation = Quaternion(vec3(up.x(), up.y(), up.z()), angle + pi / 2.0f).matrix() * Quaternion(vec3(1, 0, 0), pi / 2.0f).matrix();
					}
				}
			}
		}
	}
}

void Ant::morePizze(Kore::vec3 position) {
	vec3i pos = gridPosition(position);
	for (int x = pos.x() - 5; x <= pos.x() + 5; ++x) {
		for (int y = pos.y() - 5; y <= pos.y() + 5; ++y) {
			for (int z = pos.z() - 5; z <= pos.z() + 5; ++z) {
				setScent(x, y, z, scentAt(x, y, z) + 5.0f);
			}
		}
	}
}

void Ant::lessPizza(Kore::vec3 position) {
	vec3i pos = gridPosition(position);
	for (int x = pos.x() - 5; x <= pos.x() + 5; ++x) {
		for (int y = pos.y() - 5; y <= pos.y() + 5; ++y) {
			for (int z = pos.z() - 5; z <= pos.z() + 5; ++z) {
				setScent(x, y, z, scentAt(x, y, z) - 5.0f);
			}
		}
	}
}

extern MeshObject* objects[];
extern KitchenObject* kitchenObjects[];
//extern TriggerCollider* triggerCollider[];
extern MeshObject* roomObjects[7];

void Ant::move(float deltaTime) {
    
    if (dead) return;
    //position = vec3(4.0f, 1.5f, 0.0f);// all ants in the microwave
    if (isDying()) {
        energy += deltaTime;
        //log(Info, "Ant dying %f", energy);
        if (energy > 0.5f) {
            antsDead ++;
            log(Info, "%i Ant dead at pos %f %f %f", antsDead, position.x(), position.y(), position.z());
            dead = true;
			rotation = Quaternion(vec4(1, 0, 0, 0), pi).matrix();
            return;
        }
    }
	if (mode == FrontWall) {
		if (intersects(vec4(0, -1, 0, 0))) {
			forward = vec4(0, 0, -1, 0);
			up = vec4(0, 1, 0, 0);
			right = vec4(-1, 0, 0, 0);
			rotation = Quaternion(vec4(0, 1, 0, 0), -pi / 2).matrix();

			mode = Floor;
			chooseScent(true);
		}
		else if (!intersects(vec4(0, 0, -1, 0))) {
			forward = vec4(0, 0, 1, 0);
			up = vec4(0, 1, 0, 0);
			right = vec4(1, 0, 0, 0);
			rotation = mat4::Identity();
			
			mode = Floor;
			chooseScent(true);
		}
	}
	else if (mode == BackWall) {
		if (intersects(vec4(0, -1, 0, 0))) {
			forward = vec4(0, 0, 1, 0);
			up = vec4(0, 1, 0, 0);
			right = vec4(1, 0, 0, 0);
			rotation = mat4::Identity();

			mode = Floor;
			chooseScent(true);
		}
	}
	else {
		if (intersects(vec4(0, 0, 1, 0))) {
			forward = vec4(0, 1, 0, 0);
			up = vec4(0, 0, -1, 0);
			right = vec4(1, 0, 0, 0);
			rotation = Quaternion(vec4(1, 0, 0, 0), -pi / 2).matrix();

			mode = FrontWall;
			chooseScent(true);
		}
		else if (!intersects(vec4(0, -1, 0, 0))) {
			forward = vec4(0, -1, 0, 0);
			up = vec4(0, 0, 1, 0);
			right = vec4(-1, 0, 0, 0);
			rotation = Quaternion(vec4(1, 0, 0, 0), -pi / 2).matrix();

			mode = BackWall;
			chooseScent(true);
		}
	}
	
	if (legRotationUp) {
		legRotation += 0.15f;
		if (legRotation > pi / 4.0f) {
			legRotationUp = false;
		}
	}
	else {
		legRotation -= 0.15f;
		if (legRotation < -pi / 4.0f) {
			legRotationUp = true;
		}
	}

	chooseScent(false);
	
	position += forward * 0.03f;
}

void Ant::moveEverybody(float deltaTime) {
	++count;
	if (count % 10 == 0) {
		Ant& ant = ants[Random::get(maxAnts - 1)];

		ant.rotation = mat4::Identity();
		ant.forward = vec4(0, 0, -1, 0);
		ant.right = vec4(1, 0, 0, 0);
		ant.up = vec4(0, 1, 0, 0);

		vec3 start(0, 1.5, 0);
		ant.position = vec3(start.x() + Random::get(-100, 100) / 100.0f, start.y(), start.z() + Random::get(-100, 100) / 100.0f); // vec3(Random::get(-100, 100) / 10.0f, -1, Random::get(-100, 100) / 10.0f);
																																	  //ants[i].rotation = Quaternion(ants[i].right, Random::get(3000.0f) / 1000.0f).matrix() * ants[i].rotation;
		ant.energy = 0;
		ant.dead = false;
	}

	for (int i = 0; i < maxAnts; ++i) {
		ants[i].move(deltaTime);
	}
}

bool Ant::intersects(vec3 dir) {
	if ((position + dir * 0.5f).y() <= -1) {
		return true;
	}
	for (unsigned oi = 0; kitchenObjects[oi] != nullptr; ++oi) {
		if (intersectsWith(kitchenObjects[oi]->body, dir) || intersectsWith(kitchenObjects[oi]->door_closed, dir)) {
			return true;
		}
	}
	for (int i = 0; roomObjects[i] != nullptr; ++i) {
		if (intersectsWith(roomObjects[i], dir)) {
			return true;
		}
	}
	return false;
}

bool Ant::intersectsWith(MeshObject* obj, vec3 dir) {
    if (obj == nullptr) return false;
    for (int k = 0; k < obj->colliderCount; ++k) {
        //float distance;
		//if (obj->collider[k] != nullptr && obj->collider[k]->IntersectsWith(position, dir, distance) && distance < .1f) {
		/*for (float offset = 0.0f; offset < 10.0f; offset += 0.1f) {
			if (obj->collider[k] != nullptr && obj->collider[k]->IsInside(position + dir * offset) && dir.y() == -1) {
				return true;
			}
		}*/

		if (obj->collider[k] != nullptr && obj->collider[k]->IsInside(position + dir * 1.0f)) {
			return true;
		}
    }
    return false;
}

bool Ant::isDying() {
    for (unsigned oi = 0; kitchenObjects[oi] != nullptr; ++oi) {
        if (kitchenObjects[oi]->triggerCollider != nullptr && kitchenObjects[oi]->triggerCollider->collider != nullptr && kitchenObjects[oi]->triggerCollider->collider->IsInside(position)) {
            if (kitchenObjects[oi]->closed)
                return true;
        }
    }
    return false;
}

void Ant::render(ConstantLocation vLocation, TextureUnit tex, mat4 view) {
	int c = 0;
	{
		float* data = vertexBuffers[1]->lock();
		c = 0;
		for (int i = 0; i < maxAnts; i++) {
			const float scale = 0.02f;
			mat4 M = mat4::Translation(ants[i].position.x(), ants[i].position.y(), ants[i].position.z()) * ants[i].rotation * mat4::RotationY(pi) * mat4::Scale(scale, scale, scale);
			setMatrix(data, i, 0, 36, M);
			setMatrix(data, i, 16, 36, calculateN(M));
			setVec4(data, i, 32, 36, vec4(1, 1, 1, 1));
			c++;
		}
		vertexBuffers[1]->unlock();
	}

	Graphics::setTexture(tex, body->image);
	VertexBuffer* vertexBuffers[2];
	vertexBuffers[0] = body->vertexBuffers[0];
	vertexBuffers[1] = ::vertexBuffers[1];
	Graphics::setVertexBuffers(vertexBuffers, 2);
	Graphics::setIndexBuffer(*body->indexBuffer);
	Graphics::drawIndexedVerticesInstanced(c);

	vec3 legsOffset = vec3(0.044f, 0.035f, 0.0f);

	{
		float* data = vertexBuffers[1]->lock();
		c = 0;
		for (int i = 0; i < maxAnts; i++) {
			const float scale = 0.02f;
			//x = 0.461 , y = 0.461, z = 0.213
			mat4 M = mat4::Translation(ants[i].position.x(), ants[i].position.y(), ants[i].position.z()) * ants[i].rotation * mat4::RotationY(pi) * mat4::Translation(0.0461f + legsOffset.x(), 0.0461f + legsOffset.y(), 0.0213f + 0.023f + legsOffset.z()) * mat4::RotationX(ants[i].legRotation) * mat4::Scale(scale, scale, scale);
			setMatrix(data, i, 0, 36, M);
			setMatrix(data, i, 16, 36, calculateN(M));
			setVec4(data, i, 32, 36, vec4(1, 1, 1, 1));
			c++;
		}
		vertexBuffers[1]->unlock();
	}

	vertexBuffers[0] = leg->vertexBuffers[0];
	Graphics::setVertexBuffers(vertexBuffers, 2);
	Graphics::setIndexBuffer(*leg->indexBuffer);
	Graphics::drawIndexedVerticesInstanced(c);

	{
		float* data = vertexBuffers[1]->lock();
		c = 0;
		// x = 0.422 , y = 0.414, z = -0.01
		for (int i = 0; i < maxAnts; i++) {
			const float scale = 0.02f;
			mat4 M = mat4::Translation(ants[i].position.x(), ants[i].position.y(), ants[i].position.z()) * ants[i].rotation * mat4::RotationY(pi) * mat4::Translation(0.0422f + legsOffset.x(), 0.0414f + legsOffset.y(), -0.001f + legsOffset.z()) * mat4::RotationX(-ants[i].legRotation) * mat4::Scale(scale, scale, scale);
			setMatrix(data, i, 0, 36, M);
			setMatrix(data, i, 16, 36, calculateN(M));
			setVec4(data, i, 32, 36, vec4(1, 1, 1, 1));
			c++;
		}
		vertexBuffers[1]->unlock();
	}

	vertexBuffers[0] = leg->vertexBuffers[0];
	Graphics::setVertexBuffers(vertexBuffers, 2);
	Graphics::setIndexBuffer(*leg->indexBuffer);
	Graphics::drawIndexedVerticesInstanced(c);
	
	{
		float* data = vertexBuffers[1]->lock();
		c = 0;
		// x = 0.407, y = 0.381 , z = -0.244
		for (int i = 0; i < maxAnts; i++) {
			const float scale = 0.02f;
			mat4 M = mat4::Translation(ants[i].position.x(), ants[i].position.y(), ants[i].position.z()) * ants[i].rotation * mat4::RotationY(pi) * mat4::Translation(0.0407 + legsOffset.x(), 0.0381f + legsOffset.y(), -0.0244f - 0.028f + legsOffset.z()) * mat4::RotationX(ants[i].legRotation) * mat4::Scale(scale, scale, scale);
			setMatrix(data, i, 0, 36, M);
			setMatrix(data, i, 16, 36, calculateN(M));
			setVec4(data, i, 32, 36, vec4(1, 1, 1, 1));
			c++;
		}
		vertexBuffers[1]->unlock();
	}

	legsOffset.x() *= -1.0f;

	vertexBuffers[0] = leg->vertexBuffers[0];
	Graphics::setVertexBuffers(vertexBuffers, 2);
	Graphics::setIndexBuffer(*leg->indexBuffer);
	Graphics::drawIndexedVerticesInstanced(c);

	{
		float* data = vertexBuffers[1]->lock();
		c = 0;
		// x = -0.461 , y = 0.461, z = 0.213
		for (int i = 0; i < maxAnts; i++) {
			const float scale = 0.02f;
			mat4 M = mat4::Translation(ants[i].position.x(), ants[i].position.y(), ants[i].position.z()) * ants[i].rotation * mat4::RotationY(pi) * mat4::Translation(-0.0461f + legsOffset.x(), 0.0461f + legsOffset.y(), 0.0213f + 0.023f + legsOffset.z()) * mat4::RotationX(-ants[i].legRotation) * mat4::RotationY(pi) * mat4::Scale(scale, scale, scale);
			setMatrix(data, i, 0, 36, M);
			setMatrix(data, i, 16, 36, calculateN(M));
			setVec4(data, i, 32, 36, vec4(1, 1, 1, 1));
			c++;
		}
		vertexBuffers[1]->unlock();
	}

	vertexBuffers[0] = leg->vertexBuffers[0];
	Graphics::setVertexBuffers(vertexBuffers, 2);
	Graphics::setIndexBuffer(*leg->indexBuffer);
	Graphics::drawIndexedVerticesInstanced(c);

	{
		float* data = vertexBuffers[1]->lock();
		c = 0;
		// x = -0.422 , y = 0.414, z = -0.01
		for (int i = 0; i < maxAnts; i++) {
			const float scale = 0.02f;
			mat4 M = mat4::Translation(ants[i].position.x(), ants[i].position.y(), ants[i].position.z()) * ants[i].rotation * mat4::RotationY(pi) * mat4::Translation(-0.0422f + legsOffset.x(), 0.0414f + legsOffset.y(), -0.001f + legsOffset.z()) * mat4::RotationX(ants[i].legRotation) * mat4::RotationY(pi) * mat4::Scale(scale, scale, scale);
			setMatrix(data, i, 0, 36, M);
			setMatrix(data, i, 16, 36, calculateN(M));
			setVec4(data, i, 32, 36, vec4(1, 1, 1, 1));
			c++;
		}
		vertexBuffers[1]->unlock();
	}

	vertexBuffers[0] = leg->vertexBuffers[0];
	Graphics::setVertexBuffers(vertexBuffers, 2);
	Graphics::setIndexBuffer(*leg->indexBuffer);
	Graphics::drawIndexedVerticesInstanced(c);
	
	{
		float* data = vertexBuffers[1]->lock();
		c = 0;
		// x = -0.407, y = 0.381 , z = -0.244
		for (int i = 0; i < maxAnts; i++) {
			const float scale = 0.02f;
			mat4 M = mat4::Translation(ants[i].position.x(), ants[i].position.y(), ants[i].position.z()) * ants[i].rotation * mat4::RotationY(pi) * mat4::Translation(-0.0407 + legsOffset.x(), 0.0381f + legsOffset.y(), -0.0244f - 0.028f + legsOffset.z()) * mat4::RotationX(-ants[i].legRotation) * mat4::RotationY(pi) * mat4::Scale(scale, scale, scale);
			setMatrix(data, i, 0, 36, M);
			setMatrix(data, i, 16, 36, calculateN(M));
			setVec4(data, i, 32, 36, vec4(1, 1, 1, 1));
			c++;
		}
		vertexBuffers[1]->unlock();
	}

	vertexBuffers[0] = leg->vertexBuffers[0];
	Graphics::setVertexBuffers(vertexBuffers, 2);
	Graphics::setIndexBuffer(*leg->indexBuffer);
	Graphics::drawIndexedVerticesInstanced(c);
}
