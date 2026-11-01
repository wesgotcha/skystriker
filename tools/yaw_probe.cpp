// 朝向探针：同一机体按 4 个偏航角摆放，确认机头指向哪个轴。
#include "raylib-cpp.hpp"
#include <cstdio>

int main() {
    raylib::Window window(1400, 900, "orientation probe");
    SetTargetFPS(60);
    raylib::Model model("models/player/Bob/glTF/Bob.gltf");
    raylib::Texture2D tex("models/player/Bob/Textures/Bob_Blue.png");
    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    }

    float tilt = 26.0f * DEG2RAD;
    float dist = 200.0f;
    raylib::Camera3D cam;
    cam.up = {0, 1, 0};
    cam.fovy = 44.0f;
    cam.projection = CAMERA_ORTHOGRAPHIC;
    cam.target = {0, 0, 0};
    cam.position = {0, (float)cos(tilt) * dist, (float)sin(tilt) * dist};

    int frame = 0;
    while (!window.ShouldClose() && frame < 3) {
        BeginDrawing();
        ClearBackground(raylib::Color{26, 36, 50, 255});
        BeginMode3D(cam);
        for (int i = 0; i < 4; i++) {
            float x = (i - 1.5f) * 11.0f;
            model.Draw({x, 0, 0}, {0, 1, 0}, 90.0f * i, {1, 1, 1}, raylib::Color::White());
        }
        EndMode3D();
        for (int i = 0; i < 4; i++) {
            char buf[64];
            snprintf(buf, sizeof buf, "yaw %d", 90 * i);
            raylib::Vector2 s = GetWorldToScreen({(i - 1.5f) * 11.0f, 0, 4.5f}, cam);
            DrawText(buf, (int)s.x - 28, (int)s.y, 20, RAYWHITE);
        }
        EndDrawing();
        if (frame == 1) TakeScreenshot("tools/probe_yaw.png");
        frame++;
    }
    return 0;
}
