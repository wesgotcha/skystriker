#pragma once
// 程序化音效（素材包里没有音频，全部用波形合成）
namespace sky::audio {

enum Sfx {
    SfxShot = 0,
    SfxHit,
    SfxExplode,
    SfxBigExplode,
    SfxBomb,
    SfxPickup,
    SfxUiMove,
    SfxUiSelect,
    SfxDamage,
    SfxWarn,
    SfxCount
};

void init();
void shutdown();
void play(int sfx, float volume = 1.0f, float pitch = 1.0f);
void applyVolumes(float master, float music, float sfx);

}  // namespace sky::audio
