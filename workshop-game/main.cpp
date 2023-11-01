
#include <stdio.h>
#include <chrono>
#include <thread>
#include <set>
#include <vector>
#include <iostream>

#include "imgui/imgui.h"
#include "imgui_impl_glfw_game.h"
#include "imgui_impl_opengl3_game.h"
#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "draw_game.h"

#include "box2d/box2d.h"

// GLFW main window pointer
GLFWwindow* g_mainWindow = nullptr;

b2World* g_world;
std::set<b2Body*> to_delete;


void debug_to_delete();
void debug_characters();

class Character {
    friend std::ostream& operator<<(std::ostream& os, const Character& rhs) {
        os << "Character{pointer:" << &rhs << ", size:" << rhs.size << ", body:" << rhs.body << "}";
        return os;
    }
public:
    Character(int p_size, float x, float y) {
        size = p_size;
        b2PolygonShape box_shape;
        box_shape.SetAsBox(size / 500.0f, size / 500.0f);
        b2FixtureDef box_fd;
        box_fd.shape = &box_shape;
        box_fd.density = 20.0f;
        box_fd.friction = 0.1f;
        b2BodyDef box_bd;
        box_bd.userData.pointer = (uintptr_t)this;
        box_bd.type = b2_dynamicBody;
        box_bd.position.Set(x, y);
        body = g_world->CreateBody(&box_bd);
        body->CreateFixture(&box_fd);
    }

    virtual ~Character() {
        std::cout << __FUNCTION__ << std::endl;
        g_world->DestroyBody(body);
    }

    /* overloaded equality operator */
    bool operator==(const Character& rhs) const {
        std::cout << __FUNCTION__ << std::endl;
        return this->body == rhs.body;
    }
    /* overloaded lower than operator */
    bool operator<(const Character& rhs) const {
        std::cout << __FUNCTION__ << ": " << this->size << " < " << rhs.size << " ?" << std::endl;
        return this->size < rhs.size;
    }

    /* On collision callback */
    void onCollision(Character* other) {
        Character* character_to_delete = *this < *other ? this : other;
        std::cout << __FUNCTION__ << std::endl;
        std::cout << "  this: " << *this << (this == character_to_delete ? " (to be deleted)" : "") << std::endl;
        std::cout << "  other: " << *other << (other == character_to_delete ? " (to be deleted)" : "") << std::endl;
        to_delete.insert(character_to_delete->body);
        debug_to_delete();
    }

    /* Get Character pointer from Body. If null, it is not a Character */
    static Character* getCharacterPointerFromBody(const b2Body* body) {
        return (Character*)body->GetUserData().pointer;
    }

private:
    b2Body* body;
    int size;
};

std::vector<std::unique_ptr<Character>> characters;

inline void debug_to_delete() {
    std::cout << __FUNCTION__ << ": " << to_delete.size() << " body(ies)" << std::endl;
    for (auto body : to_delete) {
        std::cout << "  " << body;
        const Character* const character_pointer = Character::getCharacterPointerFromBody(body);
        if (character_pointer) {
            std::cout << ": " << *character_pointer;
        }
        std::cout << std::endl;
    }
}

inline void debug_characters() {
    std::cout << __FUNCTION__ << ": " << characters.size() << " charater(s)" << std::endl;
    for (auto&& character : characters) {
        std::cout << "  " << *character << std::endl;
    }
}

class MyCollisionListener : public b2ContactListener {
public:
    void BeginContact(b2Contact* contact) override
    {
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();
        b2Body* bodyA = fixtureA->GetBody();
        b2Body* bodyB = fixtureB->GetBody();
        // delete only if dynamic bodies (not floor) and they are both Characters
        if (fixtureA->GetType() == b2_dynamicBody && fixtureB->GetType() == b2_dynamicBody) {
            Character* characterA_pointer = Character::getCharacterPointerFromBody(bodyA);
            Character* characterB_pointer = Character::getCharacterPointerFromBody(bodyB);
            if (characterA_pointer && characterB_pointer) {
                std::cout << "Collision between characters happened" << std::endl;
                characterA_pointer->onCollision(characterB_pointer);
                std::cout << std::endl;
            }
        }
    }
    void EndContact(b2Contact* contact) override
    {
        b2Fixture *fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();
        b2Body* bodyA = fixtureA->GetBody();
        b2Body* bodyB = fixtureB->GetBody();
        if (fixtureA->GetType() == b2_dynamicBody && fixtureB->GetType() == b2_dynamicBody) {
            if (Character::getCharacterPointerFromBody(bodyA) && Character::getCharacterPointerFromBody(bodyB)) {
                std::cout << "Collision between characters ceased\n";
            }
        }
    }
};

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // code for keys here https://www.glfw.org/docs/3.3/group__keys.html
    // and modifiers https://www.glfw.org/docs/3.3/group__mods.html
}

void MouseMotionCallback(GLFWwindow*, double xd, double yd)
{
    // get the position where the mouse was pressed
    b2Vec2 ps((float)xd, (float)yd);
    // now convert this position to Box2D world coordinates
    b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);
}

void MouseButtonCallback(GLFWwindow* window, int32 button, int32 action, int32 mods)
{
    // code for mouse button keys https://www.glfw.org/docs/3.3/group__buttons.html
    // and modifiers https://www.glfw.org/docs/3.3/group__buttons.html
    // action is either GLFW_PRESS or GLFW_RELEASE
    double xd, yd;
    // get the position where the mouse was pressed
    glfwGetCursorPos(g_mainWindow, &xd, &yd);
    b2Vec2 ps((float)xd, (float)yd);
    // now convert this position to Box2D world coordinates
    b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);

    if (action == GLFW_PRESS) {
        std::cout << __FUNCTION__ << std::endl;
        characters.push_back(std::make_unique<Character>(yd, pw.x, pw.y));
        debug_characters();
        std::cout << std::endl;
    }
}

void clear_bodies() {
    if (!to_delete.empty()) {
        std::cout << "Deleting objects scheduled to be deleted..." << std::endl;
        debug_to_delete();
        for (auto itb = to_delete.begin(); itb != to_delete.end();)
        {
            // find and delete character
            Character* character_pointer = Character::getCharacterPointerFromBody(*itb);
            if (character_pointer) {
                // this is a Character
                auto same_character = [character_pointer](std::unique_ptr<Character>& c) {
                    return *c == *character_pointer;
                    };
                auto const& itc = std::find_if(characters.begin(), characters.end(), same_character);
                if (itc != characters.end()) {
                    std::cout << "Found character to delete: " << *(*itc) << std::endl;
                    itc->reset();
                    std::cout << "Reset pointer to character" << std::endl;
                    characters.erase(itc);
                    std::cout << "Removed character from list" << std::endl;
                    debug_characters();
                }
            }
            else {
                // Destroy body from world
                g_world->DestroyBody(*itb);
                std::cout << "Deleted body" << std::endl;

            }
            // Remove body from to_delete
            itb = to_delete.erase(itb);
            std::cout << "Removed from to_delete list" << std::endl;
            debug_to_delete();
            std::cout << std::endl;
        }
    }
}

int main()
{
    // glfw initialization things
    if (glfwInit() == 0) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    g_mainWindow = glfwCreateWindow(g_camera.m_width, g_camera.m_height, "My game", NULL, NULL);

    if (g_mainWindow == NULL) {
        fprintf(stderr, "Failed to open GLFW g_mainWindow.\n");
        glfwTerminate();
        return -1;
    }

    // Set callbacks using GLFW
    glfwSetMouseButtonCallback(g_mainWindow, MouseButtonCallback);
    glfwSetKeyCallback(g_mainWindow, KeyCallback);
    glfwSetCursorPosCallback(g_mainWindow, MouseMotionCallback);

    glfwMakeContextCurrent(g_mainWindow);

    // Load OpenGL functions using glad
    int version = gladLoadGL(glfwGetProcAddress);

    // Setup Box2D world and with some gravity
    b2Vec2 gravity;
    gravity.Set(0.0f, -10.0f);
    g_world = new b2World(gravity);

    // Set collision callbacks
    MyCollisionListener collision;
    g_world->SetContactListener(&collision);

    // Create debug draw. We will be using the debugDraw visualization to create
    // our games. Debug draw calls all the OpenGL functions for us.
    g_debugDraw.Create();
    g_world->SetDebugDraw(&g_debugDraw);
    CreateUI(g_mainWindow, 20.0f /* font size in pixels */);


    // Some starter objects are created here, such as the ground
    b2Body* ground;
    b2EdgeShape ground_shape;
    ground_shape.SetTwoSided(b2Vec2(-40.0f, 0.0f), b2Vec2(40.0f, 0.0f));
    b2BodyDef ground_bd;
    ground = g_world->CreateBody(&ground_bd);
    ground->CreateFixture(&ground_shape, 0.0f);

    constexpr float fall_position = -30.0f;

    characters.push_back(std::make_unique<Character>(500, fall_position, 11.25f));

    // dominoes pieces
    {
		b2PolygonShape shape;
		shape.SetAsBox(0.1f, 1.0f);

		b2FixtureDef fd;
		fd.shape = &shape;
		fd.density = 20.0f;
		fd.friction = 0.1f;

		for (int i = 0; i < 60; ++i)
		{
			b2BodyDef bd;
			bd.type = b2_dynamicBody;
			bd.position.Set(fall_position + 0.7f + 1.0f * i, 1.0f);
            b2Body* piece = g_world->CreateBody(&bd);
            piece->CreateFixture(&fd);
		}
	}

    // This is the color of our background in RGB components
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Control the frame rate. One draw per monitor refresh.
    std::chrono::duration<double> frameTime(0.0);
    std::chrono::duration<double> sleepAdjust(0.0);

    // Main application loop
    while (!glfwWindowShouldClose(g_mainWindow)) {
        // Use std::chrono to control frame rate. Objective here is to maintain
        // a steady 60 frames per second (no more, hopefully no less)
        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

        glfwGetWindowSize(g_mainWindow, &g_camera.m_width, &g_camera.m_height);

        int bufferWidth, bufferHeight;
        glfwGetFramebufferSize(g_mainWindow, &bufferWidth, &bufferHeight);
        glViewport(0, 0, bufferWidth, bufferHeight);

        // Clear previous frame (avoid creates shadows)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Setup ImGui attributes so we can draw text on the screen. Basically
        // create a window of the size of our viewport.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(float(g_camera.m_width), float(g_camera.m_height)));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
        ImGui::End();

        // Enable objects to be draw
        uint32 flags = 0;
        flags += b2Draw::e_shapeBit;
        g_debugDraw.SetFlags(flags);

        // When we call Step(), we run the simulation for one frame
        float timeStep = 60 > 0.0f ? 1.0f / 60 : float(0.0f);
        g_world->Step(timeStep, 8, 3);

        /* Clear bodies scheduled to be deleted */
        clear_bodies();

        // Render everything on the screen
        g_world->DebugDraw();
        g_debugDraw.Flush();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(g_mainWindow);

        // Process events (mouse and keyboard) and call the functions we
        // registered before.
        glfwPollEvents();

        // Throttle to cap at 60 FPS. Which means if it's going to be past
        // 60FPS, sleeps a while instead of doing more frames.
        std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
        std::chrono::duration<double> target(1.0 / 60.0);
        std::chrono::duration<double> timeUsed = t2 - t1;
        std::chrono::duration<double> sleepTime = target - timeUsed + sleepAdjust;
        if (sleepTime > std::chrono::duration<double>(0)) {
            // Make the framerate not go over by sleeping a little.
            std::this_thread::sleep_for(sleepTime);
        }
        std::chrono::steady_clock::time_point t3 = std::chrono::steady_clock::now();
        frameTime = t3 - t1;

        // Compute the sleep adjustment using a low pass filter
        sleepAdjust = 0.9 * sleepAdjust + 0.1 * (target - frameTime);

    }

    // Terminate the program if it reaches here
    std::cout << "Cleaning charater list...\n";
    for (auto&& c : characters) {
        c.reset();
    }
    characters.clear();
    std::cout << "Done\n";
    glfwTerminate();
    g_debugDraw.Destroy();
    delete g_world;

    return 0;
}
