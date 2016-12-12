#pragma once

#include <Kore/Math/Vector.h>
#include <Kore/Math/Quaternion.h>
#include <Kore/Graphics/Graphics.h>

#include "Engine/MeshObject.h"
#include "Engine/DeathCollider.h"

class InstancedMeshObject;

class Ant {
public:
	static void init();
	Ant();
	void chooseScent();
	static void moveEverybody(float deltaTime);
	void move(float deltaTime);
	static void render(Kore::ConstantLocation vLocation, Kore::TextureUnit tex, Kore::mat4 view);

	Kore::vec3 position;
	Kore::vec4 forward, up, right;
	Kore::vec3 dir;
	Kore::mat4 rotation;
    
	bool goingup;
	Kore::vec3i lastGrid;
	float legRotation;
	bool legRotationUp;
private:
    bool intersectsWith(MeshObject* obj, Kore::vec3 dir);
	bool intersects(Kore::vec3 dir);
    
    bool isDying();
};
