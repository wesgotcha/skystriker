#include "Save.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace sky::save {

static Profile g_profile;
static const char* kPath = "save.dat";

Profile& data() { return g_profile; }

void load() {
    std::ifstream f(kPath);
    if (!f) return;
    std::string line;
    std::map<std::string, std::string> kv;
    while (std::getline(f, line)) {
        size_t p = line.find('=');
        if (p == std::string::npos) continue;
        kv[line.substr(0, p)] = line.substr(p + 1);
    }
    auto gi = [&](const char* k, int d) { auto it = kv.find(k); return it == kv.end() ? d : atoi(it->second.c_str()); };
    auto gf = [&](const char* k, float d) {
        auto it = kv.find(k);
        return it == kv.end() ? d : (float)atof(it->second.c_str());
    };
    g_profile.hiScore = gi("hi", 0);
    g_profile.lastScore = gi("last", 0);
    g_profile.kills = gi("kills", 0);
    g_profile.runs = gi("runs", 0);
    g_profile.craft = gi("craft", 0);
    g_profile.skin = gi("skin", 0);
    g_profile.craft2 = gi("craft2", 0);
    g_profile.skin2 = gi("skin2", 2);
    g_profile.stars[0] = gi("star0", 0);
    g_profile.stars[1] = gi("star1", 0);
    g_profile.stars[2] = gi("star2", 0);
    for (int i = 0; i < 3; i++) {
        g_profile.stageScore[i] = gi(("ss" + std::to_string(i)).c_str(), 0);
        g_profile.stageKills[i] = gi(("sk" + std::to_string(i)).c_str(), 0);
        g_profile.stageTime[i] = gi(("st" + std::to_string(i)).c_str(), 0);
    }
    g_profile.volMaster = gf("vm", 80.0f);
    g_profile.volMusic = gf("vmu", 45.0f);
    g_profile.volSfx = gf("vs", 85.0f);
    g_profile.shake = gi("shake", 1) != 0;
    g_profile.reduceFlash = gi("flash", 0) != 0;
    g_profile.keyFire = gi("kfire", 'J');
    g_profile.keyBomb = gi("kbomb", 'K');
}

void save() {
    std::ofstream f(kPath);
    if (!f) return;
    f << "hi=" << g_profile.hiScore << "\n";
    f << "last=" << g_profile.lastScore << "\n";
    f << "kills=" << g_profile.kills << "\n";
    f << "runs=" << g_profile.runs << "\n";
    f << "craft=" << g_profile.craft << "\n";
    f << "skin=" << g_profile.skin << "\n";
    f << "craft2=" << g_profile.craft2 << "\n";
    f << "skin2=" << g_profile.skin2 << "\n";
    f << "star0=" << g_profile.stars[0] << "\n";
    f << "star1=" << g_profile.stars[1] << "\n";
    f << "star2=" << g_profile.stars[2] << "\n";
    for (int i = 0; i < 3; i++) {
        f << "ss" << i << "=" << g_profile.stageScore[i] << "\n";
        f << "sk" << i << "=" << g_profile.stageKills[i] << "\n";
        f << "st" << i << "=" << g_profile.stageTime[i] << "\n";
    }
    f << "vm=" << g_profile.volMaster << "\n";
    f << "vmu=" << g_profile.volMusic << "\n";
    f << "vs=" << g_profile.volSfx << "\n";
    f << "shake=" << (g_profile.shake ? 1 : 0) << "\n";
    f << "flash=" << (g_profile.reduceFlash ? 1 : 0) << "\n";
    f << "kfire=" << g_profile.keyFire << "\n";
    f << "kbomb=" << g_profile.keyBomb << "\n";
}

void reset() {
    g_profile = Profile{};
    save();
}

}  // namespace sky::save
