# SKY STRIKER · 全套像素风场景素材包（Godot 4.x）

复古像素风 2.5D 横版飞机大战的 **4 个场景**全套背景素材 + 透明 Logo。
所有滚动层（海面、云带、雨丝）均已做水平无缝；物件均为透明底；统一
16-bit 色板、清晰方形像素颗粒，与 4 张游戏参考图同一套视觉。

## 目录结构

```
SkyStrikerAssets/
├── README.md                       本文件
├── parallax_scroll.gd              物件池脚本（岛屿 / 航母 / 要塞）
├── logo/
│   └── logo_sky_striker.png        透明背景 SKY STRIKER 标题（1299×437）
├── scenes/
│   ├── 01_ocean_battle/            白天海洋战斗（图A）  7 张
│   ├── 02_menu_dusk/               黄昏主菜单（图B）    4 张
│   ├── 03_boss_storm/              雷暴 Boss 战（图C）  5 张
│   └── 04_hangar/                  结算机库（图D）      2 张
└── previews/                       4 个场景的分层合成预览
```

## 素材清单

### 01_ocean_battle（白天海洋战斗）
| 文件 | 尺寸 | 透明 | 用法 |
|---|---|---|---|
| `sky_bg.png` | 1920×1080 | 否 | 静态天空，scale 0 |
| `sea_tile.png` | 2048×512 | 是 | 海面，顶部渐变海平线，mirroring (2048,0) |
| `clouds_far.png` | 2048×293 | 是 | 远云，scale 0.25 |
| `clouds_near.png` | 2048×432 | 是 | 近云，scale 1.6 |
| `island_1.png` / `island_2.png` | 620×389 / 620×411 | 是 | 对象池 |
| `carrier.png` | 860×417 | 是 | 航母编队，对象池 |

### 02_menu_dusk（黄昏主菜单）
| 文件 | 尺寸 | 透明 | 用法 |
|---|---|---|---|
| `sky_dusk.png` | 1920×1080 | 否 | 黄昏星空→橙红晚霞，静态，scale 0 |
| `clouds_dusk_far.png` | 2048×330 | 是 | 金色远云，scale 0.2 |
| `clouds_dusk_near.png` | 2048×438 | 是 | 金色近云，scale 1.4 |
| `carrier_dusk.png` | 860×407 | 是 | 夕阳航母剪影，对象池或静态装饰 |

### 03_boss_storm（雷暴 Boss 战）
| 文件 | 尺寸 | 透明 | 用法 |
|---|---|---|---|
| `sky_storm.png` | 1920×1080 | 否 | 雷暴夜空+闪电，静态，scale 0 |
| `clouds_storm_far.png` | 2048×219 | 是 | 暗紫远云，scale 0.25 |
| `clouds_storm_near.png` | 2048×480 | 是 | 暗紫近云（顶部羽化），scale 1.5 |
| `rain_tile.png` | 2048×512 | 是 | 斜向雨丝，水平+垂直均无缝，mirroring (2048,512) |
| `fortress.png` | 900×608 | 是 | 敌方移动要塞，对象池/Boss 站位 |

### 04_hangar（结算机库，静态界面）
| 文件 | 尺寸 | 透明 | 用法 |
|---|---|---|---|
| `hangar_bg.png` | 1920×1080 | 否 | 现代深灰蓝涂层甲板，右侧近景大升降台、左侧机库建筑，无飞机/UI，直接做结算页背景 |
| `player_fighter.png` | 740×408 | 是 | 蓝白红玩家主机（机头朝右），叠放在右侧升降台中央 |
| `parked_fighter.png` | 520×285 | 是 | 蓝灰小型友机，可摆放在远处甲板做装饰 |

> `hangar_bg.png` 是静态构图：相机机位与参考图 D 一致，升降台做大、做近，
> 玩家机叠上去即可成立。参考摆放：`player_fighter.png` 缩放至约 740 宽，
> 左上角贴在 (1000, 487)（1920×1080）。

### logo
- `logo_sky_striker.png`：从主菜单图抠出的金属银字 + 青色发光描边标题，
  透明底，深色背景上效果最佳；可缩放后直接放在主菜单 / 标题栏。

## 项目设置（像素风）

- 项目设置 → 通用 → 窗口：1920×1080，拉伸模式 `canvas_items`。
- 渲染 → 默认纹理过滤：`Nearest`；每张 PNG 导入确认 Filter=Nearest、压缩 Lossless。
- 滚动速度取帧率整数倍：60 FPS 用 `180 px/s`（3 px/帧），相机开 `pixel_snap`，避免半像素糊边。

## 通用场景树

```
ParallaxBackground
├── LayerSky        motion_scale=(0,0)     静态天空
├── LayerCloudsFar  motion_scale=(0.2,0)   motion_mirroring=(2048,0)
├── LayerMidProp    motion_scale=(0.45,0)  航母/要塞（对象池，不 mirroring）
├── LayerGround     motion_scale=(1,0)     海面 mirroring=(2048,0)；机库无此层
├── LayerProps      motion_scale=(1,0)     岛屿（对象池，不 mirroring）
├── LayerGameplay   motion_scale=(1,0)     玩家/敌机/子弹/弹幕
├── LayerRain       motion_scale=(2.2,0)   仅 Boss 场景，mirroring=(2048,512)
└── LayerCloudsNear motion_scale=(1.5,0)   motion_mirroring=(2048,0)
```

- 平铺层（云带 / 海面 / 雨丝）设 `motion_mirroring`，Godot 自动复制循环，无需代码。
- 物件（岛 / 航母 / 要塞）挂 `parallax_scroll.gd`，在检查器拖入纹理即可右生左收循环。

## 相机固定时手动滚动

```gdscript
@onready var pb: ParallaxBackground = $ParallaxBackground
@export var base_speed: float = 180.0
func _process(delta: float) -> void:
    pb.scroll_offset.x -= base_speed * delta
```

## 雨丝层（Boss 场景专属）

- `rain_tile.png` 水平、垂直都做了无缝：`motion_mirroring=(2048, 512)` 可铺满全屏。
- 雨丝本身半透明，建议再把该层 `modulate` 设为 `Color(1,1,1,0.6~0.85)` 调节雨量。
- 滚动速度设快（scale 2.2 左右），形成从左向右横扫的速度感。

## 机库结算界面（不滚动）

`hangar_bg.png` 本身就是完整透视甲板（现代涂层 + 右侧近景大升降台 + 左侧机库），
直接作为 `TextureRect`（全屏）或 `Sprite2D` 背景；`player_fighter.png` 叠在
右侧升降台中央，`parked_fighter.png` 可放在远处甲板做装饰。结算面板（星星、
分数、按钮）用 Godot UI 控件另做，不要画进背景。
