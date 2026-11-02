#include "Assets.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>

namespace sky::assets {

const char* kSkinName[SKIN_COUNT] = {"BLUE", "GREEN", "ORANGE", "PURPLE", "RED"};

const int kSkinSlots[kSkinSlotCount] = {SKIN_BLUE, SKIN_ORANGE, SKIN_PURPLE};

int normalizeSkin(int skin) {
    for (int i = 0; i < kSkinSlotCount; i++)
        if (kSkinSlots[i] == skin) return skin;
    return kSkinSlots[0];
}

Texture2D logoTex{};
Texture2D oceanSky{}, oceanSea{}, oceanCloudFar{}, oceanCloudNear{}, islandA{}, islandB{}, carrierSea{};
Texture2D duskSky{}, duskCloudFar{}, duskCloudNear{}, carrierDusk{};
Texture2D stormSky{}, stormCloudFar{}, stormCloudNear{}, stormRain{}, fortress{};
Texture2D hangarBg{};
Shader    unlit{};

std::vector<Craft> players;
std::vector<Craft> enemies;
Craft              boss;

static Font fBody[3]{};
static Font fTitle[2]{};
static const int  kBodySize[3] = {22, 34, 52};
static const int  kTitleSize[2] = {40, 76};

// ---------------------------------------------------------------- TTF cmap
namespace {

uint16_t Rd16(const unsigned char* p) { return (uint16_t)((p[0] << 8) | p[1]); }
uint32_t Rd32(const unsigned char* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

bool WantedCp(int c) {
    if (c >= 32 && c <= 126) return true;
    if (c >= 0x2000 && c <= 0x206F) return true;  // 通用标点
    if (c >= 0x3000 && c <= 0x303F) return true;  // 中日韩标点
    if (c >= 0x4E00 && c <= 0x9FFF) return true;  // 汉字
    if (c >= 0xFF00 && c <= 0xFF65) return true;  // 全角
    if (c >= 0x00B0 && c <= 0x00FF) return true;  // Latin-1 补充
    return false;
}

std::vector<int> CollectCodepoints(const char* path) {
    std::vector<int> out;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        TraceLog(LOG_WARNING, "FONT: cannot open %s", path);
        return out;
    }
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (buf.size() < 12) return out;
    const unsigned char* d = buf.data();
    int numTables = Rd16(d + 4);
    int cmapOff = -1;
    for (int i = 0; i < numTables; i++) {
        const unsigned char* rec = d + 12 + i * 16;
        if (rec + 16 > d + buf.size()) break;
        if (memcmp(rec, "cmap", 4) == 0) {
            cmapOff = (int)Rd32(rec + 8);
            break;
        }
    }
    if (cmapOff < 0 || cmapOff + 4 > (int)buf.size()) return out;

    const unsigned char* cm = d + cmapOff;
    int nSub = Rd16(cm + 2);
    int best4 = -1, best12 = -1;
    for (int i = 0; i < nSub; i++) {
        const unsigned char* rec = cm + 4 + i * 8;
        if (rec + 8 > d + buf.size()) break;
        int pid = Rd16(rec), eid = Rd16(rec + 2);
        int off = (int)Rd32(rec + 4);
        if (off <= 0 || cmapOff + off + 2 > (int)buf.size()) continue;
        int fmt = Rd16(cm + off);
        if (pid == 3 && eid == 10 && fmt == 12) best12 = off;
        else if (pid == 3 && eid == 1 && fmt == 4) { if (best4 < 0) best4 = off; }
        else if (pid == 0 && fmt == 4 && best4 < 0) best4 = off;
        else if (pid == 0 && fmt == 12 && best12 < 0) best12 = off;
    }

    if (best12 > 0) {
        const unsigned char* t = cm + best12;
        uint32_t nGroups = Rd32(t + 12);
        if (nGroups > 20000) nGroups = 20000;
        for (uint32_t g = 0; g < nGroups; g++) {
            const unsigned char* gr = t + 16 + g * 12;
            if (gr + 12 > d + buf.size()) break;
            uint32_t s = Rd32(gr), e = Rd32(gr + 4);
            if (e < s) continue;
            if (e - s > 40000) e = s + 40000;
            for (uint32_t c = s; c <= e; c++) {
                if (WantedCp((int)c)) out.push_back((int)c);
                if (out.size() > 2400) return out;
            }
        }
    } else if (best4 > 0) {
        const unsigned char* t = cm + best4;
        int segX2 = Rd16(t + 6);
        int seg = segX2 / 2;
        const unsigned char* endCode = t + 14;
        const unsigned char* startCode = endCode + segX2 + 2;
        for (int i = 0; i < seg; i++) {
            if (startCode + i * 2 + 2 > d + buf.size()) break;
            int s = Rd16(startCode + i * 2), e = Rd16(endCode + i * 2);
            if (s == 0xFFFF) continue;
            for (int c = s; c <= e; c++) {
                if (WantedCp(c)) out.push_back(c);
                if (out.size() > 2400) return out;
            }
        }
    }
    return out;
}

Font LoadUiFont(const char* path, int size, const std::vector<int>& cps) {
    Font f = LoadFontEx(path, size, const_cast<int*>(cps.data()), (int)cps.size());
    if (f.texture.id == 0) {
        TraceLog(LOG_WARNING, "FONT: fallback for %s", path);
        f = GetFontDefault();
    } else {
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    }
    return f;
}

Texture2D LoadTex(const char* path, bool pixelArt = false) {
    Texture2D t = LoadTexture(path);
    if (t.id == 0) {
        TraceLog(LOG_WARNING, "TEX: missing %s", path);
        return t;
    }
    SetTextureFilter(t, pixelArt ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
    // 双线性采样在贴图边缘会越界取到"另一头"的像素（默认 wrap 是 REPEAT）：
    // 海面顶层是透明渐隐、底层是不透明深水，取整时就会在 y=300 的海平线上
    // 横着拉出一条 1px 的亮/暗细线。贴图都是整块 quad 画的（平铺是多次绘制），
    // 不需要 REPEAT，统一 CLAMP 即可。
    SetTextureWrap(t, TEXTURE_WRAP_CLAMP);
    return t;
}

Texture2D LoadSkin(const char* path, int targetSize) {
    Image img = LoadImage(path);
    if (!img.data) {
        TraceLog(LOG_WARNING, "SKIN: missing %s", path);
        return Texture2D{};
    }
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (img.width > targetSize) ImageResize(&img, targetSize, targetSize);
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    return t;
}

// 机型资料
struct CraftDef {
    const char* id;
    const char* role;
    const char* desc;
    int         stat[5];
};

const CraftDef kPlayerDefs[6] = {
    {"Bob", "均衡型", "攻守一体，标准机体", {6, 6, 6, 6, 6}},
    {"Challenger", "突击型", "火力高，装甲厚", {5, 8, 8, 4, 5}},
    {"Dispatcher", "速射型", "射速快，弹幕密", {6, 5, 3, 7, 9}},
    {"Executioner", "重装型", "装甲高，机动弱", {3, 9, 9, 3, 4}},
    {"Omen", "高速型", "机动高，游走作战", {9, 5, 4, 9, 6}},
    {"Striker", "标准型", "全能型，全面均衡", {7, 6, 6, 7, 6}},
};

const CraftDef kEnemyDefs[4] = {
    {"Imperial", "重甲机", "厚甲，扇形弹幕", {3, 7, 9, 2, 3}},
    {"Insurgent", "游走机", "高速曲线，单发点射", {8, 5, 4, 8, 6}},
    {"Pancake", "重型机", "体型宽大，全向弹幕", {2, 8, 8, 1, 2}},
    {"Spitfire", "突击机", "高速突袭，密集弹幕", {9, 6, 3, 7, 7}},
};

}  // namespace

// ---------------------------------------------------------------- 机型加载
struct Job {
    bool        isPlayer;
    int         index;
    const char* gltf;
    const char* skinPath[SKIN_COUNT];
    int         skinSize;
};

static std::vector<Job> jobs;

static std::string ModelPath(const char* group, const char* name) {
    std::string s = "models/";
    s += group; s += "/"; s += name; s += "/glTF/"; s += name;
    // tools/prep_assets.py 生成的去贴图版本：LoadModel 不必再解码那张用不到的内嵌 2048² 贴图。
    // 没生成过就退回原版，功能不受影响，只是启动慢一点。
    std::string opt = s + "_mesh.gltf";
    if (FileExists(opt.c_str())) return opt;
    return s + ".gltf";
}
static std::string SkinPath(const char* group, const char* name, const char* skin) {
    std::string s = "models/";
    s += group; s += "/"; s += name; s += "/Textures/";
    // 同理：预处理过的是目标尺寸（512 / 1024），省掉运行时"解码 2048² 再缩"的 80ms
    std::string opt = s + "opt/" + name + "_" + skin + ".png";
    if (FileExists(opt.c_str())) return opt;
    return s + name + "_" + skin + ".png";
}

// 路径字符串集中存放：先全部构造完，再取 c_str()，避免 vector 重分配导致悬垂
static std::vector<std::string> g_pathStore;

static void BuildJobs() {
    jobs.clear();
    g_pathStore.clear();
    g_pathStore.reserve(512);

    const char* skins[SKIN_COUNT] = {"Blue", "Green", "Orange", "Purple", "Red"};

    auto addJob = [&](const char* group, const char* name, bool isPlayer, int index, int skinSize) {
        size_t base = g_pathStore.size();
        g_pathStore.push_back(ModelPath(group, name));
        for (int k = 0; k < SKIN_COUNT; k++) g_pathStore.push_back(SkinPath(group, name, skins[k]));
        Job j{};
        j.isPlayer = isPlayer;
        j.index = index;
        j.gltf = g_pathStore[base].c_str();
        for (int k = 0; k < SKIN_COUNT; k++) j.skinPath[k] = g_pathStore[base + 1 + k].c_str();
        j.skinSize = skinSize;
        jobs.push_back(j);
    };

    for (int i = 0; i < 6; i++) addJob("player", kPlayerDefs[i].id, true, i, 512);
    for (int i = 0; i < 4; i++) addJob("enemy", kEnemyDefs[i].id, false, i, 512);
    addJob("boss", "Zenith", false, 99, 1024);
}

int  modelJobCount() { return (int)jobs.size(); }
bool modelJobLoaded(int i) {
    if (i < 0 || i >= (int)jobs.size()) return false;
    const Job& j = jobs[i];
    if (j.isPlayer) return players[j.index].loaded;
    if (j.index == 99) return boss.loaded;
    return enemies[j.index].loaded;
}

void loadModelJob(int i) {
    if (i < 0 || i >= (int)jobs.size()) return;
    if (modelJobLoaded(i)) return;
    const Job& j = jobs[i];

    Craft* c = nullptr;
    if (j.isPlayer) c = &players[j.index];
    else if (j.index == 99) c = &boss;
    else c = &enemies[j.index];

    c->jobIndex = i;
    double t0 = (double)GetTime();
    c->model = LoadModel(j.gltf);
    double t1 = (double)GetTime();
    if (c->model.meshCount == 0) {
        TraceLog(LOG_ERROR, "MODEL: failed %s", j.gltf);
        c->loaded = true;
        return;
    }
    c->bb = GetModelBoundingBox(c->model);
    c->span = std::max(0.001f, c->bb.max.x - c->bb.min.x);
    c->len = std::max(0.001f, c->bb.max.z - c->bb.min.z);

    // 收集原贴图，稍后释放
    std::vector<unsigned int> oldTex;
    for (int m = 0; m < c->model.materialCount; m++) {
        Material& mat = c->model.materials[m];
        unsigned int oldId = mat.maps[MATERIAL_MAP_DIFFUSE].texture.id;
        if (oldId > 0) oldTex.push_back(oldId);
        mat.shader = unlit;
    }
    // 只预载默认配色：单张 2048 贴图解码约 75ms，5 张就是 375ms，
    // 11 架机体全量预载会把启动屏拖到 5 秒以上。其余配色在真正用到时再解。
    c->skin[SKIN_BLUE] = LoadSkin(j.skinPath[SKIN_BLUE], j.skinSize);
    c->skinLoaded[SKIN_BLUE] = (c->skin[SKIN_BLUE].id != 0);
    for (int m = 0; m < c->model.materialCount; m++) {
        c->model.materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = c->skin[SKIN_BLUE];
    }
    for (unsigned int id : oldTex) {
        Texture2D t{};
        t.id = id;
        UnloadTexture(t);
    }
    c->loaded = true;
    if (getenv("SKY_LOAD_LOG")) {
        double t2 = (double)GetTime();
        printf("[model] %-46s gltf=%5.1f skin=%5.1f ms\n", j.gltf, (t1 - t0) * 1000.0,
               (t2 - t1) * 1000.0);
        fflush(stdout);
    }
}

void ensureSkin(Craft& c, int skin) {
    if (skin < 0 || skin >= SKIN_COUNT) return;
    if (c.skinLoaded[skin] || c.skin[skin].id != 0) return;
    if (c.jobIndex < 0 || c.jobIndex >= (int)jobs.size()) return;
    c.skin[skin] = LoadSkin(jobs[c.jobIndex].skinPath[skin], jobs[c.jobIndex].skinSize);
    c.skinLoaded[skin] = (c.skin[skin].id != 0);
}

bool ensureLoaded(int job) {
    if (job < 0 || job >= (int)jobs.size()) return false;
    if (!modelJobLoaded(job)) loadModelJob(job);
    return modelJobLoaded(job);
}

// ---------------------------------------------------------------- 着色器
static const char* kVS = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
uniform mat4 matModel;
uniform mat4 matView;
uniform mat4 matProjection;
out vec2 fragTexCoord;
out vec3 fragNormal;
void main() {
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(mat3(matModel) * vertexNormal);
    gl_Position = matProjection * matView * matModel * vec4(vertexPosition, 1.0);
}
)";

static const char* kFS = R"(#version 330
in vec2 fragTexCoord;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;
void main() {
    vec4 tex = texture(texture0, fragTexCoord);
    if (tex.a < 0.03) discard;
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 L1 = normalize(vec3(-0.42, 0.92, 0.36));
    vec3 L2 = normalize(vec3(0.55, 0.30, -0.62));
    float d1 = max(dot(n, L1), 0.0);
    float d2 = max(dot(n, L2), 0.0);
    float lit = 0.58 + 0.62 * d1 + 0.14 * d2;
    finalColor = vec4(tex.rgb * lit, tex.a) * colDiffuse;
}
)";

void init() {
    logoTex = LoadTex("assets/logo/logo_sky_striker.png");

    oceanSky      = LoadTex("assets/scenes/01_ocean_battle/sky_bg.png");
    oceanSea      = LoadTex("assets/scenes/01_ocean_battle/sea_tile.png");
    oceanCloudFar = LoadTex("assets/scenes/01_ocean_battle/clouds_far.png");
    oceanCloudNear = LoadTex("assets/scenes/01_ocean_battle/clouds_near.png");
    islandA       = LoadTex("assets/scenes/01_ocean_battle/island_1.png");
    islandB       = LoadTex("assets/scenes/01_ocean_battle/island_2.png");
    carrierSea    = LoadTex("assets/scenes/01_ocean_battle/carrier.png");

    duskSky        = LoadTex("assets/scenes/02_menu_dusk/sky_dusk.png");
    duskCloudFar   = LoadTex("assets/scenes/02_menu_dusk/clouds_dusk_far.png");
    duskCloudNear  = LoadTex("assets/scenes/02_menu_dusk/clouds_dusk_near.png");
    carrierDusk    = LoadTex("assets/scenes/02_menu_dusk/carrier_dusk.png");

    stormSky        = LoadTex("assets/scenes/03_boss_storm/sky_storm.png");
    stormCloudFar   = LoadTex("assets/scenes/03_boss_storm/clouds_storm_far.png");
    stormCloudNear  = LoadTex("assets/scenes/03_boss_storm/clouds_storm_near.png");
    stormRain       = LoadTex("assets/scenes/03_boss_storm/rain_tile.png");
    fortress        = LoadTex("assets/scenes/03_boss_storm/fortress.png");

    hangarBg = LoadTex("assets/scenes/04_hangar/hangar_bg.png");

    // 字体
    std::vector<int> cps = CollectCodepoints("assets/fonts/SourceHanSansSC-Medium-subset.ttf");
    if (cps.empty()) TraceLog(LOG_WARNING, "FONT: cmap empty, chinese may not render");
    TraceLog(LOG_INFO, "FONT: %d codepoints", (int)cps.size());
    for (int i = 0; i < 3; i++)
        fBody[i] = LoadUiFont("assets/fonts/SourceHanSansSC-Medium-subset.ttf", kBodySize[i], cps);
    for (int i = 0; i < 2; i++)
        fTitle[i] = LoadUiFont("assets/fonts/SourceHanSansSC-Bold-subset.ttf", kTitleSize[i], cps);

    unlit = LoadShaderFromMemory(kVS, kFS);

    // 机型表
    players.resize(6);
    for (int i = 0; i < 6; i++) {
        players[i].id = kPlayerDefs[i].id;
        players[i].role = kPlayerDefs[i].role;
        players[i].desc = kPlayerDefs[i].desc;
        for (int k = 0; k < 5; k++) players[i].stat[k] = kPlayerDefs[i].stat[k];
        players[i].unlocked = (i == 0);
    }
    enemies.resize(4);
    for (int i = 0; i < 4; i++) {
        enemies[i].id = kEnemyDefs[i].id;
        enemies[i].role = kEnemyDefs[i].role;
        enemies[i].desc = kEnemyDefs[i].desc;
        for (int k = 0; k < 5; k++) enemies[i].stat[k] = kEnemyDefs[i].stat[k];
    }
    boss.id = "ZENITH";
    boss.role = "要塞机";
    boss.desc = "雷暴要塞的最终兵器";
    int bs[5] = {4, 10, 10, 3, 7};
    for (int k = 0; k < 5; k++) boss.stat[k] = bs[k];

    BuildJobs();
}

void shutdown() {
    // 机体不显式释放：进程即将退出，交给系统回收，避免材质贴图重复释放
    for (auto& p : players)
        for (int k = 0; k < SKIN_COUNT; k++)
            if (p.skin[k].id) UnloadTexture(p.skin[k]);
    for (auto& e : enemies)
        for (int k = 0; k < SKIN_COUNT; k++)
            if (e.skin[k].id) UnloadTexture(e.skin[k]);
    for (int k = 0; k < SKIN_COUNT; k++)
        if (boss.skin[k].id) UnloadTexture(boss.skin[k]);

    for (auto* t : {&logoTex, &oceanSky, &oceanSea, &oceanCloudFar, &oceanCloudNear, &islandA, &islandB,
                    &carrierSea, &duskSky, &duskCloudFar, &duskCloudNear, &carrierDusk, &stormSky,
                    &stormCloudFar, &stormCloudNear, &stormRain, &fortress, &hangarBg}) {
        if (t->id) UnloadTexture(*t);
    }
    for (int i = 0; i < 3; i++) UnloadFont(fBody[i]);
    for (int i = 0; i < 2; i++) UnloadFont(fTitle[i]);
    if (unlit.id) UnloadShader(unlit);
}

Font& body(int size) {
    if (size <= kBodySize[0]) return fBody[0];
    if (size <= kBodySize[1]) return fBody[1];
    return fBody[2];
}

Font& title(int size) {
    if (size <= kTitleSize[0]) return fTitle[0];
    return fTitle[1];
}

void drawCraft(const Craft& c, float sx, float sy, int skin, const pose::Pose& p, float scalePx, Color tint,
               int facing) {
    if (!c.loaded || c.model.meshCount == 0) return;
    const Vector3 pivot{(c.bb.min.x + c.bb.max.x) * 0.5f, (c.bb.min.y + c.bb.max.y) * 0.5f,
                        (c.bb.min.z + c.bb.max.z) * 0.5f};

    Craft& mc = const_cast<Craft&>(c);
    ensureSkin(mc, skin);   // 首次用到某个配色时才解码
    if (mc.skin[skin].id) {
        for (int i = 0; i < mc.model.materialCount; i++)
            mc.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = mc.skin[skin];
    }

    // 以「最长边」为基准归一化，保证不同机型在屏幕上的体量一致
    const float s = scalePx / std::max(c.span, c.len);
    Vector3 pos{sx, 0.0f, (sy - VH * 0.5f) / CamCos() + VH * 0.5f};

    // 机头统一摆到 ±X（模型机头在 -Z，绕 Y 转 ∓90°）
    const float baseYaw = (facing >= 0) ? -90.0f : 90.0f;
    // 敌机朝 -X，姿态需要镜像，否则"拉升抬头"会变成低头
    const float rollDeg = p.roll * RAD2DEG * (float)facing;
    const float pitchDeg = p.pitch * RAD2DEG * (float)facing;

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(baseYaw, 0, 1, 0);   // 1) 摆正机头
    rlRotatef(rollDeg, 1, 0, 0);   // 2) 绕世界 X 滚转
    rlRotatef(pitchDeg, 0, 0, 1);  // 3) 绕世界 Z 俯仰
    rlScalef(s, s, s);
    rlTranslatef(-pivot.x, -pivot.y, -pivot.z);
    DrawModel(c.model, Vector3{0, 0, 0}, 1.0f, tint);
    rlPopMatrix();
}

}  // namespace sky::assets
