#include "Engine/pch.h"

#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/Math/Random.h>
#include <Kore/System.h>
#include <Kore/Input/Keyboard.h>
#include <Kore/Input/Mouse.h>
#include <Kore/Audio/Mixer.h>
#include <Kore/Graphics/Image.h>
#include <Kore/Graphics/Graphics.h>
#include <Kore/Log.h>

#include "Engine/Collision.h"
#include "Engine/ObjLoader.h"
#include "Engine/Particles.h"
#include "Engine/PhysicsObject.h"
#include "Engine/PhysicsWorld.h"
#include "Engine/Rendering.h"

using namespace Kore;

namespace {
	const int width = 1024;
	const int height = 768;

	double startTime;
	Shader* vertexShader;
	Shader* fragmentShader;
	Program* program;

	float cameraAngle = 0.0f;

	bool left;
	bool right;
	bool up;
	bool down;

	// null terminated array of MeshObject pointers
	MeshObject* staticObjects[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	mat4 P;
	mat4 View;
	mat4 PV;

	vec3 cameraPosition;
	vec3 targetCameraPosition;
	vec3 oldCameraPosition;

	vec3 lookAt;
	vec3 targetLookAt;
	vec3 oldLookAt;

	float lightPosX;
	float lightPosY;
	float lightPosZ;

	MeshObject* sphere;
	PhysicsObject* po;

	PhysicsWorld physics;
	
	TextureUnit tex;
	ConstantLocation pLocation;
	ConstantLocation vLocation;
	ConstantLocation mLocation;
	ConstantLocation nLocation;
	ConstantLocation lightPosLocation;
	ConstantLocation tintLocation;

	BoxCollider boxCollider(vec3(-46.0f, -4.0f, 44.0f), vec3(10.6f, 4.4f, 4.0f));

	Texture* particleImage;
	ParticleSystem* particleSystem;

	double lastTime;

	void update() {
		double t = System::time() - startTime;
		double deltaT = t - lastTime;

		lastTime = t;
		Kore::Audio::update();
		
		Graphics::begin();
		Graphics::clear(Graphics::ClearColorFlag | Graphics::ClearDepthFlag, 0xff9999FF, 1000.0f);

		// Important: We need to set the program before we set a uniform
		program->set();
		Graphics::setFloat4(tintLocation, vec4(1, 1, 1, 1));
		Graphics::setBlendingMode(SourceAlpha, Kore::BlendingOperation::InverseSourceAlpha);
		Graphics::setRenderState(BlendingState, true);

		// Reset tint for objects that should not be tinted
		Graphics::setFloat4(tintLocation, vec4(1, 1, 1, 1));

		// set the camera
		cameraAngle += 0.3f * deltaT;

		float x = 0 + 10 * Kore::cos(cameraAngle);
		float z = 0 + 10 * Kore::sin(cameraAngle);
		
		targetCameraPosition.set(x, 2, z);

		targetCameraPosition = physics.physicsObjects[0]->GetPosition();
		targetCameraPosition = targetCameraPosition + vec3(-10, 5, 10);
		vec3 targetLookAt = physics.physicsObjects[0]->GetPosition();

		// Interpolate the camera to not follow small physics movements
		float alpha = 0.3f;

		cameraPosition = oldCameraPosition * (1.0f - alpha) + targetCameraPosition * alpha;
		oldCameraPosition = cameraPosition;

		lookAt = oldLookAt * (1.0f - alpha) + targetLookAt * alpha;
		oldLookAt = lookAt;
		
		// Follow the ball with the camera
		P = mat4::Perspective(0.5f * pi, (float)width / (float)height, 0.1f, 100);
		View = mat4::lookAt(cameraPosition, lookAt, vec3(0, 1, 0)); 

		Graphics::setMatrix(pLocation, P);
		Graphics::setMatrix(vLocation, View);

		// update light pos
		lightPosX = 20 * Kore::sin(2 * t);
		lightPosY = 10;
		lightPosZ = 20 * Kore::cos(2 * t);
		Graphics::setFloat3(lightPosLocation, lightPosX, lightPosY, lightPosZ);

		// Handle inputs
		float forceX = 0.0f;
		float forceZ = 0.0f;
		if (up) forceX += 1.0f;
		if (down) forceX -= 1.0f;
		if (left) forceZ -= 1.0f;
		if (right) forceZ += 1.0f;

		// Apply inputs
		PhysicsObject** currentP = &physics.physicsObjects[0];
		vec3 force(forceX, 0.0f, forceZ);
		force = force * 20.0f;
		(*currentP)->ApplyForceToCenter(force);

		// Update physics
		physics.Update(deltaT);

		// Check for game over
		PhysicsObject* SpherePO = physics.physicsObjects[0];
		bool result = SpherePO->Collider.IntersectsWith(boxCollider);
		if (result) {
			// ...
		}

		// Render dynamic objects
		currentP = &physics.physicsObjects[0];
		while (*currentP != nullptr) {
			(*currentP)->UpdateMatrix();
			(*currentP)->Mesh->render(mLocation, nLocation, tex);
			++currentP;
		}

		// Render static objects
		MeshObject** current = &staticObjects[0];
		while (*current != nullptr) {
			(*current)->render(mLocation, nLocation, tex);
			++current;
		}

		// Update and render particles
		particleSystem->setPosition(SpherePO->GetPosition());
		particleSystem->update(deltaT);
		particleSystem->render(tex, particleImage, vLocation, mLocation, nLocation, tintLocation, View);

		Graphics::end();
		Graphics::swapBuffers();
	}

	void SpawnSphere(vec3 Position, vec3 Velocity) {
		PhysicsObject* po = new PhysicsObject(false, 1.0f);
		po->SetPosition(Position);
		po->Velocity = Velocity;
		
		po->Collider.radius = 0.5f;

		po->Mass = 5;
		po->Mesh = sphere;
		
		// The impulse should carry the object forward
		po->ApplyImpulse(Velocity);
		physics.AddObject(po);
	}

	void keyDown(KeyCode code, wchar_t character) {
		if (code == Key_Up) {
			up = true;
		} else if (code == Key_Down) {
			down = true;
		} else if (code == Key_Left) {
			right = true;
		} else if (code == Key_Right) {
			left = true;
		}
	}

	void keyUp(KeyCode code, wchar_t character) {
		if (code == Key_Up) {
			up = false;
		} else if (code == Key_Down) {
			down = false;
		} else if (code == Key_Left) {
			right = false;
		} else if (code == Key_Right) {
			left = false;
		}
	}

	void mouseMove(int windowId, int x, int y, int movementX, int movementY) {

	}
	
	void mousePress(int windowId, int button, int x, int y) {

	}

	void mouseRelease(int windowId, int button, int x, int y) {
		
	}

	void init() {
		FileReader vs("shader.vert");
		FileReader fs("shader.frag");
		vertexShader = new Shader(vs.readAll(), vs.size(), VertexShader);
		fragmentShader = new Shader(fs.readAll(), fs.size(), FragmentShader);

		// This defines the structure of your Vertex Buffer
		VertexStructure structure;
		structure.add("pos", Float3VertexData);
		structure.add("tex", Float2VertexData);
		structure.add("nor", Float3VertexData);

		program = new Program;
		program->setVertexShader(vertexShader);
		program->setFragmentShader(fragmentShader);
		program->link(structure);

		tex = program->getTextureUnit("tex");
		pLocation = program->getConstantLocation("P");
		vLocation = program->getConstantLocation("V");
		mLocation = program->getConstantLocation("M");
		nLocation = program->getConstantLocation("N");
		lightPosLocation = program->getConstantLocation("lightPos");
		tintLocation = program->getConstantLocation("tint");

		staticObjects[0] = new MeshObject("level.obj", "level.png", structure);
		
		sphere = new MeshObject("cube.obj", "cube.png", structure);

		float pos = -10.0f;
		SpawnSphere(vec3(-pos, 5.5f, pos), vec3(0, 0, 0));

		physics.meshCollider.mesh = staticObjects[0];

		/*Sound* winSound;
		winSound = new Sound("sound.wav");
		Mixer::play(winSound);*/

		Graphics::setRenderState(DepthTest, true);
		Graphics::setRenderState(DepthTestCompare, ZCompareLess);

		Graphics::setTextureAddressing(tex, U, Repeat);
		Graphics::setTextureAddressing(tex, V, Repeat);

		particleImage = new Texture("particle.png", true);
		particleSystem = new ParticleSystem(vec3(-pos, 5.5f, pos), 3.0f, vec4(2.5f, 0, 0, 1), vec4(0, 0, 0, 0), 100, structure);
	}
}

int kore(int argc, char** argv) {
	Kore::System::setName("Korerorinpa");
	Kore::System::setup();

	Kore::WindowOptions options;
	options.title = "Korerorinpa";
	options.width = width;
	options.height = height;
	options.x = 100;
	options.y = 100;
	options.targetDisplay = -1;
	options.mode = WindowModeWindow;
	options.rendererOptions.depthBufferBits = 16;
	options.rendererOptions.stencilBufferBits = 8;
	options.rendererOptions.textureFormat = 0;
	options.rendererOptions.antialiasing = 0;
	Kore::System::initWindow(options);

	Kore::Mixer::init();
	Kore::Audio::init();

	init();

	Kore::System::setCallback(update);

	startTime = System::time();

	Keyboard::the()->KeyDown = keyDown;
	Keyboard::the()->KeyUp = keyUp;
	Mouse::the()->Move = mouseMove;
	Mouse::the()->Press = mousePress;
	Mouse::the()->Release = mouseRelease;

	Kore::System::start();

	return 0;
}
