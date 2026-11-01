extends Node2D
## 视差背景物件池（小岛 / 航母等无法平铺的大件）
## 挂在 ParallaxLayer 下（该层 motion_scale 建议 1.0，不设 motion_mirroring）。
## 从屏幕右侧生成、随背景向左移动，完全移出左侧后回收循环。
## Godot 4.x

@export_group("移动")
## 与本层 ParallaxLayer.motion_scale 配合后的世界移动速度（像素/秒）
@export var scroll_speed: float = 180.0
## 与同层海面的相对速度倍数；远景航母可设小一些（如 0.5）
@export var speed_factor: float = 1.0

@export_group("生成")
@export var min_interval: float = 2.5
@export var max_interval: float = 5.0
## 物件在画面中的随机纵向范围（屏幕坐标，0 为屏幕顶部）
@export var y_min: float = 700.0
@export var y_max: float = 980.0
@export var scale_min: float = 0.8
@export var scale_max: float = 1.15
## 右边界之外的提前量（像素）
@export var spawn_margin: float = 200.0

@export_group("素材")
@export var textures: Array[Texture2D] = []

var _pool: Array[Sprite2D] = []
var _timer: float = 1.0
var _screen_w: float = 1920.0


func _ready() -> void:
	_screen_w = get_viewport_rect().size.x
	# 每种纹理预建 2 个实例进池
	for tex in textures:
		for _i in 2:
			var s := Sprite2D.new()
			s.texture = tex
			s.visible = false
			s.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
			add_child(s)
			_pool.append(s)
	_timer = randf_range(min_interval, max_interval)


func _process(delta: float) -> void:
	# 已激活物件向左移动
	var v := scroll_speed * speed_factor * delta
	for s in _pool:
		if s.visible:
			s.position.x -= v
			var half_w: float = s.texture.get_width() * s.scale.x * 0.5
			if s.position.x < -_screen_w * 0.5 - half_w:
				s.visible = false

	# 定时生成
	_timer -= delta
	if _timer <= 0.0:
		_spawn()
		_timer = randf_range(min_interval, max_interval)


func _spawn() -> void:
	# 找一个空闲实例
	var free: Array[Sprite2D] = _pool.filter(func(s): return not s.visible)
	if free.is_empty():
		return
	var s: Sprite2D = free[0]
	var sc: float = randf_range(scale_min, scale_max)
	s.scale = Vector2(sc, sc)
	s.position = Vector2(
		_screen_w * 0.5 + spawn_margin,
		randf_range(y_min, y_max)
	)
	s.visible = true
