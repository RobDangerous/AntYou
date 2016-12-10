#include "pch.h"

#include <vector>
#include <algorithm>
#include <iostream>

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
#include "Engine/InstancedMeshObject.h"
#include "Engine/ObjLoader.h"
#include "Engine/Particles.h"
#include "Engine/PhysicsObject.h"
#include "Engine/PhysicsWorld.h"
#include "Engine/Rendering.h"
#include "Engine/Explosion.h"
#include "Landscape.h"
#include "Text.h"

#include "Projectiles.h"
#include "TankSystem.h"
#include "Tank.h"

#include "Ant.h"

using namespace Kore;

namespace {
	Ant* ant;

	const int width = 1024;
	const int height = 768;
	const int MAX_DESERTED = 5;
	const int START_DELAY = 8;
    const float CAMERA_ROTATION_SPEED = 0.001f;

	int mouseX = width / 2;
	int mouseY = height / 2;

	double startTime;
	Shader* vertexShader;
	Shader* fragmentShader;
	Program* program;

	Shader* instancedVertexShader;
	Shader* instancedFragmentShader;
	Program* instancedProgram;

	float cameraZoom = 0.5f;

	bool left;
	bool right;
	bool up;
	bool down;

	Kravur* font14;
	Kravur* font24;
	Kravur* font34;
	Kravur* font44;
	Text* textRenderer;

	mat4 P;
	mat4 View;

	vec3 cameraPosition;
    vec3 cameraRotation;
	vec3 lookAt;

	float lightPosX;
	float lightPosY;
	float lightPosZ;
    
    bool rotate;
    
    float mousePressX;
    float mousePressY;

	InstancedMeshObject* stoneMesh;
	MeshObject* projectileMesh;
	ConstantLocation instancedPLocation;
	ConstantLocation instancedVLocation;
	TextureUnit instancedTex;

    // null terminated array of MeshObject pointers
    MeshObject* objects[10];// = { nullptr, nullptr, nullptr, nullptr, nullptr };

	Projectiles* projectiles;

	PhysicsWorld physics;

	TextureUnit tex;
	ConstantLocation pLocation;
	ConstantLocation vLocation;
    ConstantLocation mLocation;
	ConstantLocation lightPosLocation;

	BoxCollider boxCollider(vec3(-46.0f, -4.0f, 44.0f), vec3(10.6f, 4.4f, 4.0f));

	Texture* particleImage;
    Explosion* explosionSystem;

	double lastTime;
    double gameOverTime = 0;
	bool gameOver = false;
	int gameOverKills = 0;

	InstancedMeshObject* tankTop;
	InstancedMeshObject* tankBottom;
    InstancedMeshObject* tankFlag;
	TankSystem* tankTics;
    ParticleRenderer* particleRenderer;

	Ground* ground;

	vec3 screenToWorld(vec2 screenPos) {
		vec4 pos((2 * screenPos.x()) / width - 1.0f, -((2 * screenPos.y()) / height - 1.0f), 0.0f, 1.0f);

		mat4 projection = P;
		mat4 view = View;

		projection = projection.Invert();
		view = view.Invert();

		vec4 worldPos = view * projection * pos;
		return vec3(worldPos.x() / worldPos.w(), worldPos.y() / worldPos.w(), worldPos.z() / worldPos.w());
	}

	float inline clamp(float min, float max, float val) {
		return Kore::max(min, Kore::min(max, val));
	}

	float inline clamp01(float val) {
		return Kore::max(0.0f, Kore::min(1.0f, val));
	}

	void renderShadowText(char* s,  float w, float h) {
		int offset = textRenderer->font->size / 12;
		textRenderer->drawString(s, 0x000000aa, w + offset, h + offset, mat3::Identity());
		textRenderer->drawString(s, 0xffffffff, w, h, mat3::Identity());
	}

	void renderCentered(char* s, float h, float w = width / 2) {
		float l = textRenderer->font->stringWidth(s);
		renderShadowText(s, w - l / 2, h);
	}

	void update() {
		double t = System::time() - startTime;
		double deltaT = t - lastTime;

		lastTime = t;
		Kore::Audio::update();

		Graphics::begin();
		Graphics::clear(Graphics::ClearColorFlag | Graphics::ClearDepthFlag | Graphics::ClearStencilFlag, 0xFF000000, 1.0f, 0);

		// Important: We need to set the program before we set a uniform
		program->set();
		Graphics::setBlendingMode(SourceAlpha, Kore::BlendingOperation::InverseSourceAlpha);
		Graphics::setRenderState(BlendingState, true);
		Graphics::setRenderState(DepthTest, true);

		if (t >= START_DELAY) {
			const float cameraSpeed = 1.0f;
			if (mouseY < 50) {
				cameraPosition.z() += cameraSpeed * clamp01(1 - mouseY / 50.0f);
			}
			if (up) {
				cameraPosition.z() += cameraSpeed;
			}
			if (mouseY > height - 50) {
				cameraPosition.z() -= cameraSpeed * clamp01((mouseY - height + 50) / 50.0f);
			}
			if (down) {
				cameraPosition.z() -= cameraSpeed;
			}
			if (mouseX < 50) {
				cameraPosition.x() -= cameraSpeed * clamp01(1 - mouseX / 50.0f);
			}
			if (right) {
				cameraPosition.x() -= cameraSpeed;
			}
			if (mouseX > width - 50) {
				cameraPosition.x() += cameraSpeed * clamp01((mouseX - width + 50) / 50.0f);
			}
			if (left) {
				cameraPosition.x() += cameraSpeed;
			}
		}
		else {
			//cameraZoom = t / START_DELAY;
		}
        
        P = mat4::Perspective(45, (float)width / (float)height, 0.1f, 1000);
        
        cameraPosition.z() = cameraZoom * 150 + (1 - cameraZoom) * 10;
        vec3 off = vec3(0, -1, 0) * cameraZoom + (1 - cameraZoom) * vec3(0, -1, 1);
        vec3 lookAt = cameraPosition + vec3(0, 0, -1);
        
        View = mat4::lookAt(cameraPosition, lookAt, vec3(0, -1, 0));
        View *= mat4::Rotation(cameraRotation.x(), cameraRotation.y(), cameraRotation.z());

		Graphics::setMatrix(pLocation, P);
		Graphics::setMatrix(vLocation, View);

		// update light pos
		/*lightPosX = 100;
		lightPosY = 100;
		lightPosZ = 100;
		Graphics::setFloat3(lightPosLocation, lightPosX, lightPosY, lightPosZ);

		if (t >= START_DELAY) {
			projectiles->update(deltaT);
			physics.Update(deltaT);
			tankTics->update(deltaT);
		}

		renderLandscape(tex);
		tankTics->render(tex, View, vLocation);*/
        
        // render the kitchen
        MeshObject** current = &objects[0];
        while (*current != nullptr) {
            // set the model matrix
            Graphics::setMatrix(mLocation, (*current)->M);
            (*current)->render(tex);
            
            ++current;
        }

		instancedProgram->set();
		
		Graphics::setMatrix(instancedPLocation, P);
		Graphics::setMatrix(instancedVLocation, View);
		
		ant->move();
		ant->render(instancedVLocation, instancedTex, View);

		/*projectiles->render(vLocation, tex, View);
		particleRenderer->render(tex, View, vLocation);

		textRenderer->start();
		gameOver = gameOver || tankTics->deserted >= MAX_DESERTED;
		if (t < START_DELAY) {
			textRenderer->setFont(font44);
			renderCentered("Tank You!", height / 2 - 100);
			textRenderer->setFont(font24);
			renderCentered("Make the war last forever. But be warned, experienced", height / 2 + 50);
			renderCentered("soldiers might realize that the bloodshed is pointless.", height / 2 + 100);
		}
		else if (!gameOver) {
			char c[42];
			char d[42];
			char k[42];
			sprintf(c, "Time: %i", (int)t - START_DELAY);
			sprintf(d, "Deserted: %i / %i", tankTics->deserted, MAX_DESERTED);
			sprintf(k, "Destroyed: %i", tankTics->destroyed);
			textRenderer->setFont(font24);
			renderShadowText(c, 15, 15);
			renderShadowText(k, 15, 45);
			renderShadowText(d, 15, 75);
		}
		else {
			textRenderer->setFont(font44);
			renderCentered("Game over!", height / 2 - 220);
			textRenderer->setFont(font34);
			if(gameOverTime == 0.0f)
				gameOverTime = t - START_DELAY;
			if (gameOverKills == 0) gameOverKills = tankTics->destroyed;
			char gameOverText[256];
			sprintf(gameOverText, "The war lasted %i seconds and killed %i...", (int)gameOverTime, gameOverKills);
			renderCentered(gameOverText, height / 2 - 70);
			textRenderer->setFont(font24);
			renderCentered("Tank you for playing our jam game:", height / 2 + 80);
			renderCentered("Polona Caserman", height / 2 + 140, width / 4.0f);
			renderCentered("Robert Konrad", height / 2 + 140);
			renderCentered("Lars Lotter", height / 2 + 140, width * 3.0f / 4);
			renderCentered("Max Maag", height / 2 + 200, width / 3.0f);
			renderCentered("Christian Reuter", height / 2 + 200, width * 2.0f / 3.0f);
			textRenderer->setFont(font14);
			renderCentered("Background music by Hong Linh Thai and Maria Rumjanzewa", height / 2 + 280);
		}
		textRenderer->end();*/

		Graphics::end();
		Graphics::swapBuffers();
	}

	void skipIntro() {
		if (System::time() - startTime < START_DELAY) {
			float diff = START_DELAY - (System::time() - startTime);
			startTime -= diff;
			lastTime += diff;
			cameraZoom = 1;
		}
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
        } else if (code == Key_A) {
            log(Info,"CONTROLL");
            tankTics->setMultipleSelect(true);
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
        } else if (code == Key_A) {
            tankTics->setMultipleSelect(false);
        }
	}

	void mouseMove(int windowId, int x, int y, int movementX, int movementY) {
		mouseX = x;
		mouseY = y;

		vec3 position = screenToWorld(vec2(mouseX, mouseY));
		vec3 pickDir = vec3(position.x(), position.y(), position.z()) - cameraPosition;
		pickDir.normalize();

		//tankTics->hover(cameraPosition, pickDir);
        
        if (rotate) {
            cameraRotation.x() += (float)((mousePressX - x) * CAMERA_ROTATION_SPEED);
            cameraRotation.z() += (float)((mousePressY - y) * CAMERA_ROTATION_SPEED);
            mousePressX = x;
            mousePressY = y;
        }
	}

	void mousePress(int windowId, int button, int x, int y) {
        rotate = true;
        
        mousePressX = x;
        mousePressY = y;
        
		vec3 position = screenToWorld(vec2(mouseX, mouseY));
		vec3 pickDir = vec3(position.x(), position.y(), position.z()) - cameraPosition;
		pickDir.normalize();

		if (button == 0) {
			skipIntro();
			//tankTics->select(cameraPosition, pickDir);
		}
		else if (button == 1) {
			//tankTics->issueCommand(cameraPosition, pickDir);
		}
	}

	void mouseRelease(int windowId, int button, int x, int y) {
        rotate = false;
	}

	void mouseScroll(int windowId, int delta) {
		cameraZoom = clamp(0.0f, 1.0f, cameraZoom + delta * 0.05f);
	}

	void init() {
		FileReader vs("shader.vert");
		FileReader fs("shader.frag");
		instancedVertexShader = new Shader(vs.readAll(), vs.size(), VertexShader);
		instancedFragmentShader = new Shader(fs.readAll(), fs.size(), FragmentShader);

		// This defines the structure of your Vertex Buffer
		VertexStructure** structures = new VertexStructure*[2];
		structures[0] = new VertexStructure();
		structures[0]->add("pos", Float3VertexData);
		structures[0]->add("tex", Float2VertexData);
		structures[0]->add("nor", Float3VertexData);

		structures[1] = new VertexStructure();
		structures[1]->add("M", Float4x4VertexData);
		structures[1]->add("N", Float4x4VertexData);
		structures[1]->add("tint", Float4VertexData);

		instancedProgram = new Program;
		instancedProgram->setVertexShader(instancedVertexShader);
		instancedProgram->setFragmentShader(instancedFragmentShader);
		instancedProgram->link(structures, 2);

		instancedTex = instancedProgram->getTextureUnit("tex");
		instancedPLocation = instancedProgram->getConstantLocation("P");
		instancedVLocation = instancedProgram->getConstantLocation("V");
		lightPosLocation = instancedProgram->getConstantLocation("lightPos");

		/*stoneMesh = new InstancedMeshObject("Data/Meshes/stone.obj", "Data/Textures/stone.png", structures, STONE_COUNT);
		projectileMesh = new MeshObject("Data/Meshes/projectile.obj", "Data/Textures/projectile.png", structures, PROJECTILE_SIZE);

        particleImage = new Texture("Data/Textures/particle.png", true);
        particleRenderer = new ParticleRenderer(structures);
        projectiles = new Projectiles(1000, 20, particleImage, projectileMesh, structures, &physics);*/
        
        // New shaders
        FileReader vs2("shader2.vert");
        FileReader fs2("shader2.frag");
        vertexShader = new Shader(vs2.readAll(), vs2.size(), VertexShader);
        fragmentShader = new Shader(fs2.readAll(), fs2.size(), FragmentShader);
        
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
        
        objects[0] = new MeshObject("Data/Meshes/chair.obj", "Data/Textures/map.png", structure, vec3(10.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 1.0f);   // TODO: texture
        objects[1] = new MeshObject("Data/Meshes/table.obj", "Data/Textures/map.png", structure, vec3(-10.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 1.0f);
        objects[2] = new MeshObject("Data/Meshes/microwave_body.obj", "Data/Textures/map.png", structure, vec3(-10.0f, 1.2f, 0.0f), vec3(-90.0f, 0.0f, 0.0f), 1.0f);
        objects[3] = new MeshObject("Data/Meshes/microwave_door.obj", "Data/Textures/white.png", structure, vec3(-10.0f, 1.2f, 0.0f), vec3(-90.0f, 0.0f, 0.0f), 1.0f);

		ant = new Ant;

		Graphics::setRenderState(DepthTest, true);
		Graphics::setRenderState(DepthTestCompare, ZCompareLess);

		Graphics::setTextureAddressing(tex, U, Repeat);
		Graphics::setTextureAddressing(tex, V, Repeat);


        //explosionSystem = new Explosion(vec3(2,6,0), 2.f, 10.f, 300, structures, particleImage);

		cameraPosition = vec3(0, 0, 20);
        cameraRotation = vec3(0, Kore::pi, 0);
		cameraZoom = 0.5f;

        Random::init(System::time() * 100);

		//createLandscape(structures, MAP_SIZE_OUTER, stoneMesh, STONE_COUNT, ground);

		font14 = Kravur::load("Data/Fonts/arial", FontStyle(), 14);
		font24 = Kravur::load("Data/Fonts/arial", FontStyle(), 24);
		font34 = Kravur::load("Data/Fonts/arial", FontStyle(), 34);
		font44 = Kravur::load("Data/Fonts/arial", FontStyle(), 44);
		textRenderer = new Text;
		textRenderer->setProjection(width, height);
		textRenderer->setFont(font44);

		/*tankTop = new InstancedMeshObject("Data/Meshes/tank_top.obj", "Data/Textures/tank_top.png", structures, MAX_TANKS, 8);
		tankBottom = new InstancedMeshObject("Data/Meshes/tank_bottom.obj", "Data/Textures/tank_bottom.png", structures, MAX_TANKS, 10);
		tankFlag = new InstancedMeshObject("Data/Meshes/flag.obj", "Data/Textures/flag.png", structures, MAX_TANKS, 2);

		tankTics = new TankSystem(&physics, particleRenderer, tankBottom, tankTop, tankFlag, vec3(-MAP_SIZE_INNER / 2, 6, -MAP_SIZE_INNER / 2), vec3(-MAP_SIZE_INNER / 2, 6, MAP_SIZE_INNER / 2), vec3(MAP_SIZE_INNER / 2, 6, -MAP_SIZE_INNER / 2), vec3(MAP_SIZE_INNER / 2, 6, MAP_SIZE_INNER / 2), 3, projectiles, structures, ground);

		Sound *bgSound = new Sound("Data/Sounds/WarTheme.wav");
        Mixer::play(bgSound);*/
	}
}

int kore(int argc, char** argv) {
    Kore::System::setName("Tank You!");
	Kore::System::setup();

	Kore::WindowOptions options;
	options.title = "Tank You!";
	options.width = width;
	options.height = height;
	options.x = 100;
	options.y = 0;
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
	Mouse::the()->Scroll = mouseScroll;

	Kore::System::start();

	return 0;
}
