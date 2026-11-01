// 用游戏相机测试不同 yaw 下机头指向
#include "raylib-cpp.hpp"
#include <cstdio>

int main() {
    raylib::Window w(1400, 900, "yaw2");
    SetTargetFPS(60);
    raylib::Model m("models/player/Bob/glTF/Bob.gltf");
    raylib::Texture2D t("models/player/Bob/Textures/Bob_Blue.png");
    for (int i = 0; i < m.materialCount; i++) m.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = t;
    BoundingBox bb = GetModelBoundingBox(m);
    Vector3 pivot{(bb.min.x + bb.max.x) * 0.5f, (bb.min.y + bb.max.y) * 0.5f, (bb.min.z + bb.max.z) * 0.5f};

    // 与游戏完全一致的正交相机：fovy=1080，1 单位 = 1 像素，从 +Z 上方俯视
    Camera3D cam{};
    cam.up = {0, 1, 0};
    cam.fovy = 1080;
    cam.projection = CAMERA_ORTHOGRAPHIC;
    cam.target = {960, 0, 540};
    float tilt = 26.0f * DEG2RAD;
    cam.position = {960, (float)cos(tilt) * 3000.0f, 540 + (float)sin(tilt) * 3000.0f};

    int frame = 0;
    while (!w.ShouldClose() && frame < 3) {
        BeginDrawing();
        ClearBackground(Color{24, 34, 48, 255});
        BeginMode3D(cam);
        const float yaws[4] = {0.0f, 90.0f, -90.0f, 180.0f};
        for (int i = 0; i < 4; i++) {
            float sx = 300 + i * 380.0f, sy = 400;
            float cz = cos(tilt);
            Vector3 pos{sx, 0, (sy - 540) / cz + 540};
            float s = 300.0f / (bb.max.x - bb.min.x);
            rlPushMatrix();
            rlTranslatef(pos.x, pos.y, pos.z);
            rlRotatef(yaws[i], 0, 1, 0);
            rlScalef(s, s, s);
            rlTranslatef(-pivot.x, -pivot.y, -pivot.z);
            // 在模型坐标系里画一根指向 +Z（机头）的线，随旋转变换
            DrawLine3D(Vector3{0, 0, 0}, Vector3{0, 0, 9}, RED);
            DrawModel(m, Vector3{0, 0, 0}, 1.0f, WHITE);
            rlPopMatrix();
        }
        EndMode3D();
        for (int i = 0; i < 4; i++) {
            char b[32];
            snprintf(b, sizeof b, "yaw %.0f", yaws[i]);
            DrawText(b, 200 + i * 300, 620, 24, RAYWHITE);
        }
        EndDrawing();
        if (frame == 1) TakeScreenshot("tools/probe_yaw2.png");
        frame++;
    }
    return 0;
}
