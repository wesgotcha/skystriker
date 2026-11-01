#include "Audio.hpp"

#include "Common.hpp"

#include <cmath>
#include <vector>

namespace sky::audio {

static Sound  g_sfx[SfxCount]{};
static bool   g_ready = false;
static float  g_sfxVol = 0.85f;

// ---------------------------------------------------------------- 合成工具
namespace {

struct Buf {
    std::vector<float> s;
    int rate = 44100;
    void alloc(float sec) { s.assign((size_t)(sec * rate), 0.0f); }
    void normalize(float peak = 0.85f) {
        float m = 1e-6f;
        for (float v : s) m = std::max(m, std::fabs(v));
        float k = peak / m;
        for (float& v : s) v *= k;
    }
    Wave toWave() {
        Wave w{};
        w.frameCount = (unsigned int)s.size();
        w.sampleRate = (unsigned int)rate;
        w.sampleSize = 16;
        w.channels = 1;
        short* data = (short*)MemAlloc(sizeof(short) * s.size());
        for (size_t i = 0; i < s.size(); i++) {
            float v = clampf(s[i], -1.0f, 1.0f);
            data[i] = (short)(v * 32000.0f);
        }
        w.data = data;
        return w;
    }
};

float Noise() { return (float)GetRandomValue(-10000, 10000) / 10000.0f; }

Sound Make(const Buf& b) {
    Wave w = const_cast<Buf&>(b).toWave();
    Sound snd = LoadSoundFromWave(w);
    UnloadWave(w);
    return snd;
}

}  // namespace

void init() {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "AUDIO: device not ready, running silent");
        return;
    }
    SetRandomSeed(20260101);

    // 射击：短促方波下滑
    {
        Buf b; b.alloc(0.075f);
        float ph = 0;
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            float f = 1050.0f - 620.0f * (t / 0.075f);
            ph += f / b.rate;
            float sq = (ph - std::floor(ph)) < 0.5f ? 1.0f : -1.0f;
            float env = std::exp(-t * 42.0f);
            b.s[i] = sq * env * 0.5f;
        }
        b.normalize(0.5f);
        g_sfx[SfxShot] = Make(b);
    }
    // 命中：噪声短爆
    {
        Buf b; b.alloc(0.05f);
        float lp = 0;
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            lp += 0.35f * (Noise() - lp);
            b.s[i] = lp * std::exp(-t * 70.0f);
        }
        b.normalize(0.45f);
        g_sfx[SfxHit] = Make(b);
    }
    // 爆炸：噪声 + 低频冲击
    {
        Buf b; b.alloc(0.42f);
        float lp = 0, ph = 0;
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            float k = 0.06f + 0.5f * std::exp(-t * 12.0f);
            lp += k * (Noise() - lp);
            float f = 150.0f * std::exp(-t * 6.0f) + 45.0f;
            ph += f / b.rate;
            float body = std::sin(ph * 2 * PI) * std::exp(-t * 9.0f) * 0.7f;
            b.s[i] = (lp * 0.9f + body) * std::exp(-t * 5.5f);
        }
        b.normalize(0.75f);
        g_sfx[SfxExplode] = Make(b);
    }
    // 大爆炸
    {
        Buf b; b.alloc(1.0f);
        float lp = 0, lp2 = 0, ph = 0;
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            float k = 0.03f + 0.4f * std::exp(-t * 6.0f);
            lp += k * (Noise() - lp);
            lp2 += 0.02f * (lp - lp2);
            float f = 90.0f * std::exp(-t * 3.0f) + 32.0f;
            ph += f / b.rate;
            float body = std::sin(ph * 2 * PI) * std::exp(-t * 3.5f);
            b.s[i] = (lp * 0.8f + lp2 * 2.2f + body * 0.9f) * std::exp(-t * 2.6f);
        }
        b.normalize(0.9f);
        g_sfx[SfxBigExplode] = Make(b);
    }
    // 爆弹：上扫 + 噪声
    {
        Buf b; b.alloc(0.75f);
        float ph = 0, lp = 0;
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            float f = 180.0f + 1500.0f * std::pow(t / 0.75f, 0.6f);
            ph += f / b.rate;
            lp += 0.2f * (Noise() - lp);
            float env = std::min(1.0f, t * 12.0f) * std::exp(-t * 2.2f);
            b.s[i] = (std::sin(ph * 2 * PI) * 0.55f + lp * 0.7f) * env;
        }
        b.normalize(0.8f);
        g_sfx[SfxBomb] = Make(b);
    }
    // 拾取：两音上行
    {
        Buf b; b.alloc(0.2f);
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            float f = (t < 0.07f) ? 880.0f : 1320.0f;
            float env = std::exp(-std::fmod(t, 0.1f) * 26.0f);
            b.s[i] = std::sin(t * f * 2 * PI) * env * 0.6f;
        }
        b.normalize(0.5f);
        g_sfx[SfxPickup] = Make(b);
    }
    // UI 移动
    {
        Buf b; b.alloc(0.045f);
        float ph = 0;
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            ph += 1500.0f / b.rate;
            float sq = (ph - std::floor(ph)) < 0.5f ? 1.0f : -1.0f;
            b.s[i] = sq * std::exp(-t * 55.0f) * 0.4f;
        }
        b.normalize(0.32f);
        g_sfx[SfxUiMove] = Make(b);
    }
    // UI 确认
    {
        Buf b; b.alloc(0.16f);
        float ph = 0;
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            float f = 700.0f + 900.0f * (t / 0.16f);
            ph += f / b.rate;
            float sq = (ph - std::floor(ph)) < 0.5f ? 1.0f : -1.0f;
            b.s[i] = sq * std::exp(-t * 16.0f) * 0.45f;
        }
        b.normalize(0.4f);
        g_sfx[SfxUiSelect] = Make(b);
    }
    // 受伤
    {
        Buf b; b.alloc(0.34f);
        float ph = 0;
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            float f = 260.0f * std::exp(-t * 3.2f) + 70.0f;
            ph += f / b.rate;
            float saw = (ph - std::floor(ph)) * 2.0f - 1.0f;
            b.s[i] = saw * std::exp(-t * 7.0f) * 0.7f;
        }
        b.normalize(0.6f);
        g_sfx[SfxDamage] = Make(b);
    }
    // 警告
    {
        Buf b; b.alloc(0.55f);
        for (size_t i = 0; i < b.s.size(); i++) {
            float t = i / (float)b.rate;
            bool on = (t < 0.16f) || (t > 0.28f && t < 0.44f);
            float env = on ? std::min(1.0f, (on ? 1.0f : 0.0f)) : 0.0f;
            b.s[i] = on ? std::sin(t * 620 * 2 * PI) * 0.5f * env : 0.0f;
        }
        b.normalize(0.5f);
        g_sfx[SfxWarn] = Make(b);
    }

    g_ready = true;
    applyVolumes(80.0f, 45.0f, 85.0f);
}

void shutdown() {
    if (!IsAudioDeviceReady()) return;
    for (int i = 0; i < SfxCount; i++)
        if (g_sfx[i].frameCount > 0) UnloadSound(g_sfx[i]);
    CloseAudioDevice();
}

void play(int sfx, float volume, float pitch) {
    if (!g_ready || sfx < 0 || sfx >= SfxCount) return;
    if (g_sfx[sfx].frameCount == 0) return;
    SetSoundVolume(g_sfx[sfx], clampf(volume * g_sfxVol, 0, 1));
    SetSoundPitch(g_sfx[sfx], clampf(pitch, 0.2f, 4.0f));
    PlaySound(g_sfx[sfx]);
}

void applyVolumes(float master, float music, float sfx) {
    (void)music;
    g_sfxVol = (master / 100.0f) * (sfx / 100.0f);
    if (g_sfxVol <= 0.001f) g_sfxVol = 0.0f;
}

}  // namespace sky::audio
