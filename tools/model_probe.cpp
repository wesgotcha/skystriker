// 模型探针：把 models/ 下的所有机体排成网格，从"游戏相机角度"截图，用于确认朝向与配色。
#include "raylib-cpp.hpp"
#include <string>
#include <vector>
#include <cstdio>

struct Entry { const char* path; const char* name; };

int main() {
    const std::vector<Entry> entries = {
        {"models/player/Bob/glTF/Bob.gltf", "Bob"},
        {"models/player/Challenger/glTF/Challenger.gltf", "Challenger"},
        {"models/player/Dispatcher/glTF/Dispatcher.gltf", "Dispatcher"},
        {"models/player/Executioner/glTF/Executioner.gltf", "Executioner"},
        {"models/player/Omen/glTF/Omen.gltf", "Omen"},
        {"models/player/Striker/glTF/Striker.gltf", "Striker"},
        {"models/enemy/Imperial/glTF/Imperial.gltf", "Imperial"},
        {"models/enemy/Insurgent/glTF/Insurgent.gltf", "Insurgent"},
        {"models/enemy/Pancake/glTF/Pancake.gltf", "Pancake"},
        {"models/enemy/Spitfire/glTF/Spitfire.gltf", "Spitfire"},
        {"models/boss/Zenith/glTF/Zenith.gltf", "Zenith"},
    };

    raylib::Window window(1600, 900, "model probe");
    SetTargetFPS(60);

    std::vector<raylib::Model> models;
    for (auto& e : entries) models.emplace_back(e.path);

    float tilt = 26.0f * DEG2RAD;
    float dist = 200.0f;
    raylib::Camera3D cam;
    cam.up = {0, 1, 0};
    cam.fovy = 90.0f;
    cam.projection = CAMERA_ORTHOGRAPHIC;
    cam.target = {0, 0, 0};
    cam.position = {0, (float)cos(tilt) * dist, (float)sin(tilt) * dist};

    auto cellPos = [](int i) {
        int col = i % 4, row = i / 4;
        return raylib::Vector3{(float)(col - 1.5f) * 24.0f, 0.0f, (float)(row - 1.0f) * 22.0f};
    };

    int frame = 0;
    while (!window.ShouldClose() && frame < 3) {
        BeginDrawing();
        ClearBackground(raylib::Color{26, 36, 50, 255});
        BeginMode3D(cam);
        for (size_t i = 0; i < models.size(); i++) models[i].Draw(cellPos((int)i), 1.0f, raylib::Color::White());
        EndMode3D();
        for (size_t i = 0; i < entries.size(); i++) {
            raylib::Vector2 s = GetWorldToScreen(cellPos((int)i), cam);
            DrawText(entries[i].name, (int)s.x - 40, (int)s.y + 30, 20, RAYWHITE);
        }
        EndDrawing();
        if (frame == 1) TakeScreenshot("tools/probe_oblique.png");
        frame++;
    }
    return 0;
}
