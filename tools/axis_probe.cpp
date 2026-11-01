// 轴探针：画出 +X(绿) / +Z(红) 轴，确认模型机头朝向。
#include "raylib-cpp.hpp"

int main() {
    raylib::Window window(1200, 800, "axis probe");
    SetTargetFPS(60);
    raylib::Model model("models/player/Bob/glTF/Bob.gltf");
    raylib::Texture2D tex("models/player/Bob/Textures/Bob_Blue.png");
    for (int i = 0; i < model.materialCount; i++) model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = tex;

    float tilt = 26.0f * DEG2RAD;
    raylib::Camera3D cam;
    cam.up = {0, 1, 0};
    cam.fovy = 24.0f;
    cam.projection = CAMERA_ORTHOGRAPHIC;
    cam.target = {0, 0, 0};
    cam.position = {0, (float)cos(tilt) * 200.0f, (float)sin(tilt) * 200.0f};

    int frame = 0;
    while (!window.ShouldClose() && frame < 3) {
        BeginDrawing();
        ClearBackground(raylib::Color{26, 36, 50, 255});
        BeginMode3D(cam);
        model.Draw({0, 0, 0}, 1.0f, raylib::Color::White());
        DrawLine3D({0, 0.2f, 0}, {0, 0.2f, 9}, GREEN);   // +Z
        DrawLine3D({0, 0.2f, 0}, {9, 0.2f, 0}, RED);     // +X
        DrawLine3D({0, 0.2f, 0}, {-9, 0.2f, 0}, YELLOW); // -X
        EndMode3D();
        raylib::Vector2 pz = GetWorldToScreen({0, 0.2f, 9}, cam);
        raylib::Vector2 px = GetWorldToScreen({9, 0.2f, 0}, cam);
        raylib::Vector2 nx = GetWorldToScreen({-9, 0.2f, 0}, cam);
        DrawText("+Z", (int)pz.x, (int)pz.y, 24, GREEN);
        DrawText("+X", (int)px.x, (int)px.y, 24, RED);
        DrawText("-X", (int)nx.x, (int)nx.y, 24, YELLOW);
        EndDrawing();
        if (frame == 1) TakeScreenshot("tools/probe_axis.png");
        frame++;
    }
    return 0;
}
