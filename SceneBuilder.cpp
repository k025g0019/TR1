#include "SceneBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

//========================================
// SceneBuilder 実装
//========================================
// このファイルでは、学習世界と UI 情報をすべて頂点列へ変換しています。
// 盤面、右側パネル、アイコン、テキストの見た目を段階的に積み上げる構成です。

namespace {
	//==================================
	// 内部型
	//==================================

	struct PointPx {
		float x = 0.0f;
		float y = 0.0f;
	};

	struct RectPx {
		float left = 0.0f;
		float top = 0.0f;
		float right = 0.0f;
		float bottom = 0.0f;
	};

	struct SceneLayout {
		RectPx grid;
		RectPx panel;
		RectPx metricsCard;
		RectPx legendCard;
	};

	//==================================
	// レイアウト
	//==================================

	SceneLayout BuildLayout(float width, float height, int gridWidth, int gridHeight) {
		/* 全体レイアウト計算 */
		// 画面サイズと現在マップの縦横比から、セル形状を崩さず収まる盤面矩形を決めます。
		constexpr float margin = 34.0f;
		constexpr float gap = 26.0f;
		constexpr float panelWidth = 380.0f;

		const float gridAreaWidth = width - margin * 2.0f - gap - panelWidth;
		const float gridAreaHeight = height - margin * 2.0f;
		const float cellSize = std::min(
			gridAreaWidth / static_cast<float>(std::max(1, gridWidth)),
			gridAreaHeight / static_cast<float>(std::max(1, gridHeight)));
		const float gridPixelWidth = cellSize * static_cast<float>(gridWidth);
		const float gridPixelHeight = cellSize * static_cast<float>(gridHeight);
		const float gridLeft = margin + (gridAreaWidth - gridPixelWidth) * 0.5f;
		const float gridTop = margin + (gridAreaHeight - gridPixelHeight) * 0.5f;

		SceneLayout layout = {};
		layout.grid = {gridLeft, gridTop, gridLeft + gridPixelWidth, gridTop + gridPixelHeight};
		layout.panel = {layout.grid.right + gap, margin, width - margin, height - margin};
		layout.metricsCard = {
			layout.panel.left + 18.0f,
			layout.panel.top + 18.0f,
			layout.panel.right - 18.0f,
			layout.panel.top + 430.0f,
		};
		layout.legendCard = {
			layout.panel.left + 18.0f,
			layout.metricsCard.bottom + 18.0f,
			layout.panel.right - 18.0f,
			layout.panel.bottom - 18.0f,
		};
		// 小見出し
		// こうしておくと、以後の描画関数は「どのカードを使うか」だけで座標を共有できます。
		return layout;
	}

	RectPx MetricBarRect(const SceneLayout& layout, int row) {
		/* ゲージ矩形 */
		// 指標は等間隔に縦へ並べたいので、行番号から上端をずらして同じ形のバーを作ります。
		const float top = layout.metricsCard.top + 138.0f + static_cast<float>(row) * 46.0f;
		return {
			layout.metricsCard.left + 18.0f,
			top + 18.0f,
			layout.metricsCard.right - 18.0f,
			top + 32.0f,
		};
	}

	RectPx EpisodeTargetInputRectPx(const SceneLayout& layout) {
		/* 入力欄矩形 */
		// 入力欄はメトリクスカードの下端へ寄せ、統計表示と操作欄が自然につながるように置きます。
		return {
			layout.metricsCard.left + 18.0f,
			layout.metricsCard.bottom - 60.0f,
			layout.metricsCard.right - 18.0f,
			layout.metricsCard.bottom - 20.0f,
		};
	}

	//==================================
	// 座標変換
	//==================================

	float ToClipX(float x, float width) {
		/* クリップ座標変換 X */
		return (x / width) * 2.0f - 1.0f;
	}

	float ToClipY(float y, float height) {
		return 1.0f - (y / height) * 2.0f;
	}

	Vertex MakeVertex(float x, float y, float width, float height, const Color& color) {
		/* 頂点生成 */
		// ピクセル座標と色を DirectX 用の頂点形式へ変換します。
		return {
			ToClipX(x, width),
			ToClipY(y, height),
			0.0f,
			color.r,
			color.g,
			color.b,
			color.a,
		};
	}

	//==================================
	// 基本図形
	//==================================

	void AppendTriangle(
		std::vector<Vertex>& vertices,
		const PointPx& a,
		const PointPx& b,
		const PointPx& c,
		float width,
		float height,
		const Color& color) {
		// 小見出し
		// 三角形 1 枚をそのまま頂点 3 つとして追加します。
		vertices.push_back(MakeVertex(a.x, a.y, width, height, color));
		vertices.push_back(MakeVertex(b.x, b.y, width, height, color));
		vertices.push_back(MakeVertex(c.x, c.y, width, height, color));
	}

	void AppendQuad(
		std::vector<Vertex>& vertices,
		const RectPx& rect,
		float width,
		float height,
		const Color& color) {
		// 小見出し
		// 四角形は 2 枚の三角形へ分けて表現します。
		const PointPx a{rect.left, rect.top};
		const PointPx b{rect.right, rect.top};
		const PointPx c{rect.right, rect.bottom};
		const PointPx d{rect.left, rect.bottom};
		AppendTriangle(vertices, a, c, d, width, height, color);
		AppendTriangle(vertices, a, b, c, width, height, color);
	}

	void AppendFrame(
		std::vector<Vertex>& vertices,
		const RectPx& rect,
		float thickness,
		float width,
		float height,
		const Color& color) {
		// 小見出し
		// 上下左右の細い四角形を並べて枠線を作ります。
		AppendQuad(vertices, {rect.left, rect.top, rect.right, rect.top + thickness}, width, height, color);
		AppendQuad(vertices, {rect.left, rect.bottom - thickness, rect.right, rect.bottom}, width, height, color);
		AppendQuad(vertices, {rect.left, rect.top, rect.left + thickness, rect.bottom}, width, height, color);
		AppendQuad(vertices, {rect.right - thickness, rect.top, rect.right, rect.bottom}, width, height, color);
	}

	void AppendDiamond(
		std::vector<Vertex>& vertices,
		float centerX,
		float centerY,
		float radius,
		float width,
		float height,
		const Color& color) {
		// 小見出し
		// 菱形は上下と左右の 4 点から 2 枚の三角形で作ります。
		const PointPx top{centerX, centerY - radius};
		const PointPx right{centerX + radius, centerY};
		const PointPx bottom{centerX, centerY + radius};
		const PointPx left{centerX - radius, centerY};
		AppendTriangle(vertices, top, right, bottom, width, height, color);
		AppendTriangle(vertices, top, bottom, left, width, height, color);
	}

	void AppendCross(
		std::vector<Vertex>& vertices,
		float centerX,
		float centerY,
		float radius,
		float thickness,
		float width,
		float height,
		const Color& color) {
		// 小見出し
		// 縦棒と横棒の 2 つの四角形を重ねて十字形を作ります。
		AppendQuad(
			vertices,
			{centerX - thickness, centerY - radius, centerX + thickness, centerY + radius},
			width,
			height,
			color);
		AppendQuad(
			vertices,
			{centerX - radius, centerY - thickness, centerX + radius, centerY + thickness},
			width,
			height,
			color);
	}

	void AppendSegment(
		std::vector<Vertex>& vertices,
		const PointPx& from,
		const PointPx& to,
		float thickness,
		float width,
		float height,
		const Color& color) {
		// 小見出し
		// 線分は法線方向へ厚みを持たせた細長い四角形として描きます。
		const float dx = to.x - from.x;
		const float dy = to.y - from.y;
		const float length = std::sqrt(dx * dx + dy * dy);
		if (length <= 0.0001f) {
			return;
		}

		const float nx = -dy / length;
		const float ny = dx / length;
		const float ox = nx * thickness * 0.5f;
		const float oy = ny * thickness * 0.5f;

		const PointPx a{from.x - ox, from.y - oy};
		const PointPx b{from.x + ox, from.y + oy};
		const PointPx c{to.x + ox, to.y + oy};
		const PointPx d{to.x - ox, to.y - oy};
		AppendTriangle(vertices, a, b, c, width, height, color);
		AppendTriangle(vertices, a, c, d, width, height, color);
	}

	void AppendArrow(
		std::vector<Vertex>& vertices,
		float centerX,
		float centerY,
		float size,
		Action action,
		float width,
		float height,
		const Color& color) {
		// 小見出し
		// 行動方向ごとに三角形の向きを切り替えて矢印を作ります。
		switch (action) {
		case Action::Up:
			AppendTriangle(
				vertices,
				{centerX, centerY - size},
				{centerX - size * 0.65f, centerY + size * 0.45f},
				{centerX + size * 0.65f, centerY + size * 0.45f},
				width,
				height,
				color);
			break;
		case Action::Right:
			AppendTriangle(
				vertices,
				{centerX + size, centerY},
				{centerX - size * 0.45f, centerY - size * 0.65f},
				{centerX - size * 0.45f, centerY + size * 0.65f},
				width,
				height,
				color);
			break;
		case Action::Down:
			AppendTriangle(
				vertices,
				{centerX, centerY + size},
				{centerX - size * 0.65f, centerY - size * 0.45f},
				{centerX + size * 0.65f, centerY - size * 0.45f},
				width,
				height,
				color);
			break;
		case Action::Left:
			AppendTriangle(
				vertices,
				{centerX - size, centerY},
				{centerX + size * 0.45f, centerY - size * 0.65f},
				{centerX + size * 0.45f, centerY + size * 0.65f},
				width,
				height,
				color);
			break;
		}
	}

	//==================================
	// マップ補助
	//==================================

	RectPx CellRect(const SceneLayout& layout, const QLearningGrid& world, int x, int y) {
		/* セル矩形 */
		// グリッド座標を実際の描画矩形へ変換します。
		const float cellWidth =
			(layout.grid.right - layout.grid.left) / static_cast<float>(std::max(1, world.GetGridWidth()));
		const float cellHeight =
			(layout.grid.bottom - layout.grid.top) / static_cast<float>(std::max(1, world.GetGridHeight()));
		return {
			layout.grid.left + cellWidth * static_cast<float>(x),
			layout.grid.top + cellHeight * static_cast<float>(y),
			layout.grid.left + cellWidth * static_cast<float>(x + 1),
			layout.grid.top + cellHeight * static_cast<float>(y + 1),
		};
	}

	float CellSizePx(const SceneLayout& layout, const QLearningGrid& world) {
		/* セル 1 辺の基準長 */
		// BuildLayout で正方セルになるよう合わせているので、短辺側を基準サイズとして使えます。
		const float cellWidth =
			(layout.grid.right - layout.grid.left) / static_cast<float>(std::max(1, world.GetGridWidth()));
		const float cellHeight =
			(layout.grid.bottom - layout.grid.top) / static_cast<float>(std::max(1, world.GetGridHeight()));
		return std::min(cellWidth, cellHeight);
	}

	PointPx CellCenter(const SceneLayout& layout, const QLearningGrid& world, int x, int y) {
		/* セル中心 */
		// セル矩形の中央を取り、菱形アイコンや矢印を常にマスの真ん中へそろえます。
		const RectPx rect = CellRect(layout, world, x, y);
		return {(rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f};
	}

	Color CellBaseColor(const QLearningGrid& world, int x, int y) {
		/* セル基本色 */
		// 壁や拠点は固定色、通常マスは見やすい濃淡で色付けします。
		switch (world.GetTile(x, y)) {
		case Tile::Wall:
			return {0.16f, 0.18f, 0.24f, 1.0f};
		case Tile::Start:
			return {0.13f, 0.66f, 0.88f, 1.0f};
		case Tile::Goal:
			return {0.16f, 0.77f, 0.34f, 1.0f};
		case Tile::Pit:
			return {0.84f, 0.24f, 0.24f, 1.0f};
		case Tile::Empty:
		default:
			break;
		}

		const bool brightCell = ((x + y) % 2) == 0;
		return brightCell
			       ? Color{0.08f, 0.12f, 0.18f, 1.0f}
			       : Color{0.06f, 0.10f, 0.16f, 1.0f};
	}

	Color UnitClassColor(UnitClass unitClass) {
		switch (unitClass) {
		case UnitClass::Cavalry:
			return {0.94f, 0.58f, 0.14f, 1.0f};
		case UnitClass::Infantry:
			return {0.18f, 0.73f, 0.62f, 1.0f};
		case UnitClass::Archer:
			return {0.64f, 0.44f, 0.92f, 1.0f};
		default:
			break;
		}

		return {0.90f, 0.90f, 0.90f, 1.0f};
	}

	Color FactionAccentColor(UnitFaction faction) {
		return faction == UnitFaction::Player
			       ? Color{0.20f, 0.92f, 0.36f, 1.0f}
			       : Color{1.0f, 0.24f, 0.20f, 1.0f};
	}

	Color FactionBackgroundColor(UnitFaction faction) {
		return faction == UnitFaction::Player
			       ? Color{0.04f, 0.58f, 0.16f, 0.34f}
			       : Color{0.72f, 0.06f, 0.05f, 0.34f};
	}

	Color AttackLineColor(UnitFaction faction, float alpha) {
		return faction == UnitFaction::Player
			       ? Color{0.22f, 1.0f, 0.40f, alpha}
			       : Color{1.0f, 0.22f, 0.18f, alpha};
	}

	PointPx ApplyHitShake(const PointPx& center, const BattleUnit& unit) {
		if (unit.hitShakeTimer <= 0) {
			return center;
		}

		const int directionSeed = (unit.id + unit.hitShakeTimer) % 4;
		const float offsetX = (directionSeed == 0 || directionSeed == 3) ? -3.0f : 3.0f;
		const float offsetY = (directionSeed == 0 || directionSeed == 1) ? -2.0f : 2.0f;
		return {center.x + offsetX, center.y + offsetY};
	}

	std::wstring UnitClassShortName(UnitClass unitClass) {
		switch (unitClass) {
		case UnitClass::Cavalry:
			return L"騎";
		case UnitClass::Infantry:
			return L"歩";
		case UnitClass::Archer:
			return L"弓";
		default:
			break;
		}

		return L"?";
	}

	std::wstring HeadquartersCommandText(HeadquartersCommand command) {
		/* 本部命令名 */
		// 右パネルへ短く出すため、各命令を 2〜3 文字へ圧縮します。
		switch (command) {
		case HeadquartersCommand::Balanced:
			return L"均衡";
		case HeadquartersCommand::LeftAttack:
			return L"左攻";
		case HeadquartersCommand::CenterAttack:
			return L"中攻";
		case HeadquartersCommand::RightAttack:
			return L"右攻";
		case HeadquartersCommand::Flank:
			return L"側面";
		case HeadquartersCommand::Retreat:
			return L"後退";
		default:
			break;
		}

		return L"?";
	}

	std::wstring AIIntentText(AIIntent intent) {
		switch (intent) {
		case AIIntent::Advance:
			return L"前進";
		case AIIntent::Hold:
			return L"維持";
		case AIIntent::Retreat:
			return L"退避";
		case AIIntent::Flank:
			return L"迂回";
		case AIIntent::Support:
			return L"支援";
		case AIIntent::HuntArcher:
			return L"追弓";
		case AIIntent::Kite:
			return L"射撃";
		case AIIntent::Protect:
			return L"保護";
		default:
			break;
		}
		return L"?";
	}

	std::wstring DivisionCommandText(DivisionCommand command) {
		/* 師団命令名 */
		// 左翼・中央・右翼の命令を横並びで読める短い表記へします。
		switch (command) {
		case DivisionCommand::Hold:
			return L"保持";
		case DivisionCommand::Advance:
			return L"前進";
		case DivisionCommand::Support:
			return L"支援";
		case DivisionCommand::Flank:
			return L"側面";
		case DivisionCommand::Retreat:
			return L"後退";
		default:
			break;
		}

		return L"?";
	}

	//==================================
	// パネル描画
	//==================================

	void AppendBackground(
		std::vector<Vertex>& vertices,
		const SceneLayout& layout,
		float width,
		float height) {
		// 小見出し
		// 画面全体背景、右パネル背景、右パネル枠の順に重ねます。
		AppendQuad(vertices, {0.0f, 0.0f, width, height}, width, height, {0.03f, 0.05f, 0.09f, 1.0f});
		AppendQuad(vertices, layout.panel, width, height, {0.05f, 0.08f, 0.14f, 0.97f});
		AppendFrame(vertices, layout.panel, 2.0f, width, height, {0.16f, 0.26f, 0.40f, 1.0f});
	}

	void AppendGrid(
		std::vector<Vertex>& vertices,
		const QLearningGrid& world,
		const SceneLayout& layout,
		float width,
		float height) {
		/* 盤面の外枠 */
		// まずグリッド全体の背景と枠線を描きます。
		AppendQuad(vertices, layout.grid, width, height, {0.05f, 0.07f, 0.11f, 1.0f});
		AppendFrame(vertices, layout.grid, 4.0f, width, height, {0.27f, 0.43f, 0.64f, 1.0f});

		const float cellSize = CellSizePx(layout, world);

		/* 各セルの描画 */
		// マス背景と地形アイコンをセルごとに描きます。
		for (int y = 0; y < world.GetGridHeight(); ++y) {
			for (int x = 0; x < world.GetGridWidth(); ++x) {
				const RectPx cell = CellRect(layout, world, x, y);
				const float innerPadding = std::max(1.0f, cellSize * 0.05f);
				const float wallInset = std::max(1.0f, cellSize * 0.12f);
				const float crossThickness = std::max(1.5f, cellSize * 0.045f);
				const RectPx inner = {
					cell.left + innerPadding,
					cell.top + innerPadding,
					cell.right - innerPadding,
					cell.bottom - innerPadding,
				};
				AppendQuad(vertices, inner, width, height, CellBaseColor(world, x, y));
				AppendFrame(vertices, inner, 1.5f, width, height, {0.10f, 0.14f, 0.20f, 1.0f});

				const Tile tile = world.GetTile(x, y);
				const PointPx center = CellCenter(layout, world, x, y);
				const float iconRadius = (inner.right - inner.left) * 0.18f;

				// 小見出し
				// 壁だけは塗りつぶし専用で処理し、矢印などは載せません。
				if (tile == Tile::Wall) {
					AppendQuad(
						vertices,
						{
							inner.left + wallInset, inner.top + wallInset, inner.right - wallInset,
							inner.bottom - wallInset
						},
						width,
						height,
						{0.28f, 0.31f, 0.37f, 1.0f});
					continue;
				}

				if (tile == Tile::Start) {
					AppendDiamond(vertices, center.x, center.y, iconRadius * 1.15f, width, height,
					              {0.84f, 0.97f, 1.0f, 0.96f});
					AppendDiamond(vertices, center.x, center.y, iconRadius * 0.65f, width, height,
					              {0.13f, 0.66f, 0.88f, 1.0f});
				}
				else if (tile == Tile::Goal) {
					AppendDiamond(vertices, center.x, center.y, iconRadius * 1.15f, width, height,
					              {0.91f, 1.0f, 0.92f, 0.96f});
					AppendDiamond(vertices, center.x, center.y, iconRadius * 0.70f, width, height,
					              {0.10f, 0.58f, 0.24f, 1.0f});
				}
				else if (tile == Tile::Pit) {
					AppendCross(vertices, center.x, center.y, iconRadius * 1.10f, crossThickness, width, height,
					            {1.0f, 0.92f, 0.92f, 0.96f});
					AppendDiamond(vertices, center.x, center.y, iconRadius * 0.85f, width, height,
					              {0.60f, 0.10f, 0.10f, 1.0f});
				}
			}
		}

		/* 部隊描画 */
		// 兵科色を本体色、陣営色を外側アクセントとして重ねます。
		const float glowHalfSize = cellSize * 0.29f;
		const float outerRadius = cellSize * 0.23f;
		const float innerRadius = cellSize * 0.14f;
		const float lineThickness = std::max(1.5f, cellSize * 0.05f);

		/* 将軍-配下リンク線: 配下から将軍への細い点線 */
		const float commanderLineThickness = std::max(1.0f, cellSize * 0.03f);
		for (const BattleUnit& unit : world.GetBattleUnits()) {
			if (!unit.active || unit.count <= 0 || unit.isGeneral || unit.commanderId < 0) {
				continue;
			}
			for (const BattleUnit& gen : world.GetBattleUnits()) {
				if (!gen.active || gen.count <= 0 || gen.id != unit.commanderId) {
					continue;
				}
				const PointPx fromCenter = CellCenter(layout, world, unit.position.x, unit.position.y);
				const PointPx toCenter = CellCenter(layout, world, gen.position.x, gen.position.y);
				const Color linkColor = unit.faction == UnitFaction::Player
					                       ? Color{0.20f, 0.92f, 0.36f, 0.18f}
					                       : Color{1.0f, 0.24f, 0.20f, 0.18f};
				AppendSegment(vertices, fromCenter, toCenter, commanderLineThickness, width, height, linkColor);
				break;
			}
		}

		for (const BattleAttackTrace& trace : world.GetBattleAttackTraces()) {
			const PointPx attackerCenter =
				CellCenter(layout, world, trace.attackerPosition.x, trace.attackerPosition.y);
			const PointPx targetCenter =
				CellCenter(layout, world, trace.targetPosition.x, trace.targetPosition.y);
			const float alpha =
				Clamp01(static_cast<float>(trace.timer) / 10.0f) * 0.95f;
			AppendSegment(
				vertices,
				attackerCenter,
				targetCenter,
				std::max(3.0f, cellSize * 0.08f),
				width,
				height,
				AttackLineColor(trace.attackerFaction, alpha));
		}

		/* AI意図線: 各部隊が誰を狙っているか薄く表示 */
		const float intentLineThickness = std::max(1.5f, cellSize * 0.04f);
		for (const BattleUnit& unit : world.GetBattleUnits()) {
			if (!unit.active || unit.count <= 0 || unit.targetId < 0) {
				continue;
			}
			/* targetIdに一致する部隊を探す */
			for (const BattleUnit& target : world.GetBattleUnits()) {
				if (target.id != unit.targetId || !target.active || target.count <= 0) {
					continue;
				}
				const PointPx fromCenter = CellCenter(layout, world, unit.position.x, unit.position.y);
				const PointPx toCenter = CellCenter(layout, world, target.position.x, target.position.y);
				Color intentColor = {0.60f, 0.60f, 0.70f, 0.30f};
				switch (unit.intent) {
				case AIIntent::HuntArcher:
					intentColor = {0.94f, 0.58f, 0.14f, 0.40f};
					break;
				case AIIntent::Kite:
					intentColor = {0.64f, 0.44f, 0.92f, 0.40f};
					break;
				case AIIntent::Protect:
					intentColor = {0.18f, 0.73f, 0.62f, 0.40f};
					break;
				case AIIntent::Retreat:
					intentColor = {1.0f, 0.24f, 0.20f, 0.35f};
					break;
				case AIIntent::Flank:
					intentColor = {0.94f, 0.58f, 0.14f, 0.35f};
					break;
				default:
					break;
				}
				AppendSegment(vertices, fromCenter, toCenter, intentLineThickness, width, height, intentColor);
				break;
			}
		}

		for (const BattleUnit& unit : world.GetBattleUnits()) {
			if (!unit.active || unit.count <= 0) {
				continue;
			}

			const PointPx baseCenter = CellCenter(layout, world, unit.position.x, unit.position.y);
			const PointPx unitCenter = ApplyHitShake(baseCenter, unit);
			const Color classColor = UnitClassColor(unit.unitClass);
			const Color factionColor = FactionAccentColor(unit.faction);
			const RectPx unitCell = CellRect(layout, world, unit.position.x, unit.position.y);
			const float unitCellPadding = std::max(2.0f, cellSize * 0.08f);
			const RectPx factionBackground = {
				unitCell.left + unitCellPadding,
				unitCell.top + unitCellPadding,
				unitCell.right - unitCellPadding,
				unitCell.bottom - unitCellPadding,
			};
			AppendQuad(vertices, factionBackground, width, height, FactionBackgroundColor(unit.faction));
			AppendFrame(vertices, factionBackground, std::max(1.5f, cellSize * 0.04f), width, height, factionColor);

			AppendQuad(
				vertices,
				{
					unitCenter.x - glowHalfSize,
					unitCenter.y - glowHalfSize,
					unitCenter.x + glowHalfSize,
					unitCenter.y + glowHalfSize,
				},
				width,
				height,
				{classColor.r, classColor.g, classColor.b, 0.18f});

			if (unit.unitClass == UnitClass::Cavalry) {
				AppendTriangle(
					vertices,
					{unitCenter.x, unitCenter.y - outerRadius},
					{unitCenter.x - outerRadius, unitCenter.y + outerRadius * 0.85f},
					{unitCenter.x + outerRadius, unitCenter.y + outerRadius * 0.85f},
					width,
					height,
					factionColor);
				AppendTriangle(
					vertices,
					{unitCenter.x, unitCenter.y - innerRadius},
					{unitCenter.x - innerRadius, unitCenter.y + innerRadius * 0.85f},
					{unitCenter.x + innerRadius, unitCenter.y + innerRadius * 0.85f},
					width,
					height,
					classColor);
			}
			else if (unit.unitClass == UnitClass::Infantry) {
				AppendDiamond(vertices, unitCenter.x, unitCenter.y, outerRadius, width, height, factionColor);
				AppendDiamond(vertices, unitCenter.x, unitCenter.y, innerRadius, width, height, classColor);
			}
			else {
				AppendCross(vertices, unitCenter.x, unitCenter.y, outerRadius, lineThickness, width, height,
				            factionColor);
				AppendSegment(
					vertices,
					{unitCenter.x - innerRadius, unitCenter.y + innerRadius},
					{unitCenter.x + innerRadius, unitCenter.y - innerRadius},
					lineThickness,
					width,
					height,
					classColor);
				AppendSegment(
					vertices,
					{unitCenter.x - innerRadius, unitCenter.y - innerRadius},
					{unitCenter.x + innerRadius, unitCenter.y + innerRadius},
					lineThickness,
					width,
					height,
					classColor);
			}

			if (unit.hitFlashTimer > 0) {
				const float flashAlpha =
					Clamp01(static_cast<float>(unit.hitFlashTimer) / 8.0f) * 0.60f;
				const Color flashColor = unit.hitByPlayer
					                         ? Color{1.0f, 0.18f, 0.18f, flashAlpha}
					                         : Color{1.0f, 0.38f, 0.38f, flashAlpha};
				AppendQuad(
					vertices,
					{
						unitCenter.x - glowHalfSize,
						unitCenter.y - glowHalfSize,
						unitCenter.x + glowHalfSize,
						unitCenter.y + glowHalfSize,
					},
					width,
					height,
					flashColor);
			}
		}
	}

	void AppendMetricsPanel(
		std::vector<Vertex>& vertices,
		const QLearningGrid& world,
		const SceneLayout& layout,
		float width,
		float height) {
		/* カード背景 */
		AppendQuad(vertices, layout.metricsCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
		AppendFrame(vertices, layout.metricsCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});

		/* メトリクス値計算 */
		// 合戦の勝率、報酬、残兵比を 0..1 に正規化してバーで表します。
		const int playerSoldiers = world.GetPlayerSoldierCount();
		const int enemySoldiers = world.GetEnemySoldierCount();
		const int totalSoldiers = std::max(1, playerSoldiers + enemySoldiers);
		const float playerSoldierRatio =
			static_cast<float>(playerSoldiers) / static_cast<float>(totalSoldiers);
		const float enemySoldierRatio =
			static_cast<float>(enemySoldiers) / static_cast<float>(totalSoldiers);

		const std::array<float, 4> values = {
			world.GetRecentSuccessRate(),
			NormalizeRange(world.GetAverageReward(), -8.0f, 10.0f),
			playerSoldierRatio,
			enemySoldierRatio,
		};
		constexpr std::array<Color, 4> colors = {
			Color{0.92f, 0.62f, 0.17f, 1.0f},
			Color{0.24f, 0.72f, 0.96f, 1.0f},
			Color{0.20f, 0.78f, 0.34f, 1.0f},
			Color{0.80f, 0.38f, 0.92f, 1.0f},
		};

		/* メトリクスバー描画 */
		// 小見出し
		// バーの土台、現在値、外枠を行ごとに重ねます。
		for (int row = 0; row < 4; ++row) {
			const RectPx bar = MetricBarRect(layout, row);
			const RectPx fill = {
				bar.left,
				bar.top,
				bar.left + (bar.right - bar.left) * Clamp01(values[row]),
				bar.bottom,
			};
			AppendQuad(vertices, bar, width, height, {0.13f, 0.17f, 0.25f, 1.0f});
			AppendQuad(vertices, fill, width, height, colors[row]);
			AppendFrame(vertices, bar, 1.0f, width, height, {0.22f, 0.30f, 0.41f, 1.0f});
		}

		/* 入力欄背景 */
		// 右下のエピソード目標入力欄だけは白背景で目立たせます。
		const RectPx inputRect = EpisodeTargetInputRectPx(layout);
		AppendQuad(vertices, inputRect, width, height, {0.98f, 0.99f, 1.0f, 1.0f});
		AppendFrame(vertices, inputRect, 2.0f, width, height, {0.70f, 0.75f, 0.82f, 1.0f});
	}

	void AppendLegendPanel(
		std::vector<Vertex>& vertices,
		const SceneLayout& layout,
		float width,
		float height) {
		/* 凡例カード背景 */
		AppendQuad(vertices, layout.legendCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
		AppendFrame(vertices, layout.legendCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});

		const float left = layout.legendCard.left + 22.0f;
		const float top = layout.legendCard.top + 64.0f;
		constexpr float rowGap = 42.0f;

		// 小見出し
		// 凡例のアイコンは盤面と同じ見た目で揃えています。

		/* スウォッチ枠 */
		for (int row = 0; row < 5; ++row) {
			const RectPx swatch = {
				left,
				top + rowGap * static_cast<float>(row),
				left + 54.0f,
				top + rowGap * static_cast<float>(row) + 34.0f,
			};
			AppendQuad(vertices, swatch, width, height, {0.11f, 0.15f, 0.23f, 1.0f});
			AppendFrame(vertices, swatch, 1.0f, width, height, {0.22f, 0.30f, 0.41f, 1.0f});
		}

		AppendTriangle(
			vertices,
			{left + 27.0f, top + 5.0f},
			{left + 12.0f, top + 31.0f},
			{left + 42.0f, top + 31.0f},
			width,
			height,
			UnitClassColor(UnitClass::Cavalry));

		AppendDiamond(vertices, left + 27.0f, top + rowGap + 18.0f, 14.0f, width, height,
		              UnitClassColor(UnitClass::Infantry));
		AppendCross(vertices, left + 27.0f, top + rowGap * 2.0f + 18.0f, 13.0f, 4.0f, width, height,
		            UnitClassColor(UnitClass::Archer));
		AppendDiamond(vertices, left + 27.0f, top + rowGap * 3.0f + 18.0f, 13.0f, width, height,
		              FactionAccentColor(UnitFaction::Player));
		AppendCross(vertices, left + 27.0f, top + rowGap * 4.0f + 18.0f, 13.0f, 4.0f, width, height,
		            FactionAccentColor(UnitFaction::Enemy));

		/* 相性サンプル */
		// 下部には兵科相性を線で示します。
		const RectPx routeBox = {
			layout.legendCard.left + 18.0f,
			layout.legendCard.bottom - 54.0f,
			layout.legendCard.right - 18.0f,
			layout.legendCard.bottom - 18.0f,
		};
		AppendFrame(vertices, routeBox, 1.0f, width, height, {0.22f, 0.30f, 0.41f, 1.0f});

		const PointPx a{routeBox.left + 16.0f, routeBox.bottom - 11.0f};
		const PointPx b{routeBox.left + 82.0f, routeBox.top + 16.0f};
		const PointPx c{routeBox.right - 20.0f, routeBox.top + 15.0f};
		AppendSegment(vertices, a, b, 6.0f, width, height, UnitClassColor(UnitClass::Cavalry));
		AppendSegment(vertices, b, c, 6.0f, width, height, UnitClassColor(UnitClass::Archer));
		AppendArrow(vertices, b.x, b.y, 9.0f, Action::Right, width, height, {0.03f, 0.05f, 0.09f, 0.95f});
	}

	//==================================
	// 文字描画
	//==================================

	std::wstring FormatWFloat(float value, int precision) {
		/* 小数文字列化 */
		std::wostringstream stream;
		stream << std::fixed << std::setprecision(precision) << value;
		return stream.str();
	}

	std::wstring FormatEpisodeTargetInput(const EpisodeRunUiState& episodeRunUiState) {
		/* 入力欄表示文字列 */
		std::wstring text = episodeRunUiState.inputText;
		if (episodeRunUiState.editing) {
			text += L"|";
		}
		return text;
	}

	std::wstring FormatEpisodeTargetStatus(const EpisodeRunUiState& episodeRunUiState) {
		/* 入力状態メッセージ */
		if (episodeRunUiState.editing) {
			return L"数字を入力して Enter で開始";
		}

		if (episodeRunUiState.autoRunning && episodeRunUiState.targetEpisode >= 0) {
			std::wostringstream stream;
			stream << L"合戦 " << episodeRunUiState.targetEpisode << L" 回まで自動実行中";
			return stream.str();
		}

		if (episodeRunUiState.targetEpisode >= 0) {
			std::wostringstream stream;
			stream << L"クリックして数字入力 / Enter で " << episodeRunUiState.targetEpisode << L" 回";
			return stream.str();
		}

		return L"白い欄をクリックして数字入力";
	}

	HFONT CreateUiFont(int height, int weight) {
		/* UI フォント作成 */
		return CreateFontW(
			-height,
			0,
			0,
			0,
			weight,
			FALSE,
			FALSE,
			FALSE,
			DEFAULT_CHARSET,
			OUT_OUTLINE_PRECIS,
			CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE,
			L"Yu Gothic UI");
	}

	void DrawTextLine(
		HDC dc,
		int x,
		int y,
		const std::wstring& text,
		COLORREF color,
		HFONT font) {
		// 小見出し
		// 影付き 2 回描画で文字を見やすくしています。
		const auto oldFont = static_cast<HFONT>(SelectObject(dc, font));
		SetTextColor(dc, RGB(10, 14, 22));
		TextOutW(dc, x + 1, y + 1, text.c_str(), static_cast<int>(text.size()));
		SetTextColor(dc, color);
		TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
		SelectObject(dc, oldFont);
	}

	void DrawFlatTextLine(
		HDC dc,
		int x,
		int y,
		const std::wstring& text,
		COLORREF color,
		HFONT font) {
		// 小見出し
		// 影なしの単純な文字描画です。
		const auto oldFont = static_cast<HFONT>(SelectObject(dc, font));
		SetTextColor(dc, color);
		TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
		SelectObject(dc, oldFont);
	}

	void DrawWrappedText(
		HDC dc,
		RECT rect,
		const std::wstring& text,
		COLORREF color,
		HFONT font) {
		// 小見出し
		// 説明文のような複数行テキストを折り返しながら描きます。
		const auto oldFont = static_cast<HFONT>(SelectObject(dc, font));
		RECT shadowRect = rect;
		OffsetRect(&shadowRect, 1, 1);
		SetTextColor(dc, RGB(10, 14, 22));
		DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &shadowRect, DT_WORDBREAK | DT_LEFT);
		SetTextColor(dc, color);
		DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, DT_WORDBREAK | DT_LEFT);
		SelectObject(dc, oldFont);
	}

	void AppendTextBitmapGeometry(
		std::vector<Vertex>& vertices,
		const SceneLayout& layout,
		const QLearningGrid& world,
		const std::wstring& speedLabel,
		bool paused,
		const EpisodeRunUiState& episodeRunUiState,
		float width,
		float height) {
		/* テキスト用ビットマップ準備 */
		// GDI で文字を描いたあと、色付きピクセルだけを頂点化して重ねます。
		const int bitmapWidth = static_cast<int>(layout.panel.right - layout.panel.left);
		const int bitmapHeight = static_cast<int>(layout.panel.bottom - layout.panel.top);

		BITMAPINFO bitmapInfo = {};
		bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmapInfo.bmiHeader.biWidth = bitmapWidth;
		bitmapInfo.bmiHeader.biHeight = -bitmapHeight;
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 32;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;

		// 小見出し
		// まずメモリ上の 32bit ビットマップへ文字を描く準備をします。
		void* rawPixels = nullptr;
		HDC memoryDc = CreateCompatibleDC(nullptr);
		HBITMAP bitmap = CreateDIBSection(memoryDc, &bitmapInfo, DIB_RGB_COLORS, &rawPixels, nullptr, 0);
		HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

		RECT fillRect = {0, 0, bitmapWidth, bitmapHeight};
		FillRect(memoryDc, &fillRect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
		SetBkMode(memoryDc, TRANSPARENT);

		/* フォント作成 */
		// タイトル、本文、小さめ説明の 3 種類を用意します。
		HFONT titleFont = CreateUiFont(26, FW_BOLD);
		HFONT bodyFont = CreateUiFont(18, FW_NORMAL);
		HFONT smallFont = CreateUiFont(16, FW_NORMAL);

		const float panelLeft = layout.panel.left;
		const float panelTop = layout.panel.top;
		auto localX = [panelLeft](float absoluteX) {
			return static_cast<int>(absoluteX - panelLeft);
		};
		auto localY = [panelTop](float absoluteY) {
			return static_cast<int>(absoluteY - panelTop);
		};

		// 小見出し
		// ここから右側パネルの文字列を順番に描画していきます。

		/* 表示文言の整形 */
		// 入力欄のプレースホルダーや状態文字列をここで決めます。
		const RectPx inputRect = EpisodeTargetInputRectPx(layout);
		const bool usePlaceholder = episodeRunUiState.inputText.empty() && !episodeRunUiState.editing;
		const std::wstring inputText = usePlaceholder ? L"50" : FormatEpisodeTargetInput(episodeRunUiState);

		DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.top + 18.0f), L"合戦状況",
		             RGB(245, 250, 255), titleFont);
		DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.top + 52.0f),
		             L"合戦回数 : " + std::to_wstring(world.GetEpisodeCount()), RGB(255, 247, 210), bodyFont);
		DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.top + 80.0f),
		             L"総ターン : " + std::to_wstring(world.GetTrainingStepCount()), RGB(255, 247, 210), bodyFont);
		DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.top + 108.0f),
		             L"現在速度 : " + std::wstring(paused ? L"一時停止" : speedLabel), RGB(255, 247, 210), bodyFont);

		const std::array<std::wstring, 4> labels = {
			L"味方勝率",
			L"平均報酬",
			L"味方残兵",
			L"敵残兵",
		};
		const std::array<std::wstring, 4> values = {
			FormatWFloat(world.GetRecentSuccessRate(), 2),
			FormatWFloat(world.GetAverageReward(), 2),
			std::to_wstring(world.GetPlayerSoldierCount()) + L" 人",
			std::to_wstring(world.GetEnemySoldierCount()) + L" 人",
		};

		// 小見出し
		// 4 本のメトリクス名と値を縦に並べます。
		for (int row = 0; row < 4; ++row) {
			const float baseY = layout.metricsCard.top + 138.0f + static_cast<float>(row) * 46.0f;
			DrawTextLine(
				memoryDc,
				localX(layout.metricsCard.left + 18.0f),
				localY(baseY),
				labels[row] + L" : " + values[row],
				RGB(255, 247, 210),
				bodyFont);
		}

		DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.bottom - 88.0f),
		             L"目標合戦回数", RGB(255, 247, 210), bodyFont);
		DrawFlatTextLine(
			memoryDc,
			localX(inputRect.left + 14.0f),
			localY(inputRect.top + 7.0f),
			inputText,
			usePlaceholder ? RGB(120, 126, 138) : RGB(28, 31, 38),
			bodyFont);
		DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.bottom - 16.0f),
		             FormatEpisodeTargetStatus(episodeRunUiState), RGB(215, 229, 246), smallFont);

		DrawTextLine(memoryDc, localX(layout.legendCard.left + 18.0f), localY(layout.legendCard.top + 18.0f), L"兵科と指揮",
		             RGB(245, 250, 255), titleFont);

		const std::array<std::wstring, 5> legendLabels = {
			L"騎馬 : 移動 2 / 弓兵に強い",
			L"歩兵 : 防御高い / 騎馬に強い",
			L"弓兵 : 射程 3 / 歩兵に強い",
			L"味方背景",
			L"敵背景",
		};
		const float legendTextX = layout.legendCard.left + 94.0f;
		const float legendTop = layout.legendCard.top + 58.0f;
		// 小見出し
		// 凡例ラベルも縦方向に等間隔で並べます。
		for (int row = 0; row < 5; ++row) {
			DrawTextLine(
				memoryDc,
				localX(legendTextX),
				localY(legendTop + static_cast<float>(row) * 26.0f),
				legendLabels[row],
				RGB(220, 232, 248),
				smallFont);
		}

		/* 将軍グループ情報を動的に生成 */
		auto formatGeneralGroups = [&](UnitFaction faction) {
			std::wostringstream stream;
			stream << (faction == UnitFaction::Player ? L"味方" : L"敵");
			bool hasGeneral = false;
			for (const BattleUnit& u : world.GetBattleUnits()) {
				if (!u.active || u.count <= 0 || !u.isGeneral || u.faction != faction) {
					continue;
				}
				if (hasGeneral) { stream << L" / "; }
				stream << L"将" << UnitClassShortName(u.unitClass) << L"(" << u.count << L")→";
				/* 将軍のターゲットを表示 */
				for (const BattleUnit& t : world.GetBattleUnits()) {
					if (t.id == u.targetId && t.active && t.count > 0) {
						stream << (t.faction == UnitFaction::Player ? L"味" : L"敵")
						       << UnitClassShortName(t.unitClass);
						break;
					}
				}
				hasGeneral = true;
			}
			if (!hasGeneral) {
				stream << L" 将軍なし";
			}
			return stream.str();
		};
		const std::wstring playerDivisionText = formatGeneralGroups(UnitFaction::Player);
		const std::wstring enemyDivisionText = formatGeneralGroups(UnitFaction::Enemy);

		DrawTextLine(
			memoryDc,
			localX(layout.legendCard.left + 18.0f),
			localY(layout.legendCard.bottom - 116.0f),
			L"AI意図線 : 橙=騎追弓 紫=弓射撃 青=歩保護 赤=退避",
			RGB(180, 200, 230),
			smallFont);
		DrawTextLine(
			memoryDc,
			localX(layout.legendCard.left + 18.0f),
			localY(layout.legendCard.bottom - 96.0f),
			L"相性 : 騎馬 > 弓兵 > 歩兵 > 騎馬",
			RGB(215, 229, 246),
			smallFont);
		DrawTextLine(
			memoryDc,
			localX(layout.legendCard.left + 18.0f),
			localY(layout.legendCard.bottom - 72.0f),
			std::wstring(L"本部AI : 味方") +
			HeadquartersCommandText(world.GetHeadquartersCommand(UnitFaction::Player)) +
			L" / 敵" +
			HeadquartersCommandText(world.GetHeadquartersCommand(UnitFaction::Enemy)),
			RGB(215, 229, 246),
			smallFont);
		DrawTextLine(
			memoryDc,
			localX(layout.legendCard.left + 18.0f),
			localY(layout.legendCard.bottom - 48.0f),
			playerDivisionText,
			RGB(215, 229, 246),
			smallFont);
		DrawTextLine(
			memoryDc,
			localX(layout.legendCard.left + 18.0f),
			localY(layout.legendCard.bottom - 24.0f),
			enemyDivisionText,
			RGB(215, 229, 246),
			smallFont);
		DrawTextLine(
			memoryDc,
			localX(layout.legendCard.left + 18.0f),
			localY(layout.legendCard.top + 40.0f),
			L"部隊表示 : 兵科 + 現在人数 / Space 停止 / N 1ターン",
			RGB(215, 229, 246),
			smallFont);

		/* 描いた文字の頂点化 */
		// 非黒色ピクセルだけを横方向ラン単位でまとめ、四角形として積みます。
		const auto* pixels = static_cast<const std::uint32_t*>(rawPixels);
		for (int y = 0; y < bitmapHeight; ++y) {
			int x = 0;
			while (x < bitmapWidth) {
				const std::uint32_t pixel = pixels[y * bitmapWidth + x];
				if ((pixel & 0x00FFFFFFu) == 0) {
					++x;
					continue;
				}

				const std::uint32_t runColor = pixel & 0x00FFFFFFu;
				const int startX = x;
				// 小見出し
				// 同色が続く横方向ランを 1 本の細長い四角形へまとめます。
				while (x < bitmapWidth &&
					(pixels[y * bitmapWidth + x] & 0x00FFFFFFu) == runColor) {
					++x;
				}

				const float left = layout.panel.left + static_cast<float>(startX);
				const float top = layout.panel.top + static_cast<float>(y);
				const float right = layout.panel.left + static_cast<float>(x);
				const float bottom = top + 1.0f;

				const float red = static_cast<float>((runColor >> 16) & 0xFF) / 255.0f;
				const float green = static_cast<float>((runColor >> 8) & 0xFF) / 255.0f;
				const float blue = static_cast<float>(runColor & 0xFF) / 255.0f;
				AppendQuad(vertices, {left, top, right, bottom}, width, height, {red, green, blue, 1.0f});
			}
		}

		/* GDI リソース解放 */
		SelectObject(memoryDc, oldBitmap);
		DeleteObject(bitmap);
		DeleteObject(titleFont);
		DeleteObject(bodyFont);
		DeleteObject(smallFont);
		DeleteDC(memoryDc);
	}
} // namespace

//==================================
// 公開関数
//==================================

std::vector<Vertex> BuildSceneVertices(
	const QLearningGrid& world,
	unsigned int windowWidth,
	unsigned int windowHeight,
	const std::wstring& speedLabel,
	bool paused,
	const EpisodeRunUiState& episodeRunUiState) {
	/* シーン組み立て本体 */
	// 背景 -> 盤面 -> パネル -> 文字の順で頂点を積み上げます。
	const float width = static_cast<float>(windowWidth);
	const float height = static_cast<float>(windowHeight);
	const SceneLayout layout =
		BuildLayout(width, height, world.GetGridWidth(), world.GetGridHeight());

	std::vector<Vertex> vertices;
	vertices.reserve(120000);

	// 小見出し
	// 背景 -> 盤面 -> 情報パネル -> 文字の順に積むと重なり順が自然です。

	AppendBackground(vertices, layout, width, height);
	AppendGrid(vertices, world, layout, width, height);
	AppendMetricsPanel(vertices, world, layout, width, height);
	AppendLegendPanel(vertices, layout, width, height);
	AppendTextBitmapGeometry(vertices, layout, world, speedLabel, paused, episodeRunUiState, width, height);
	return vertices;
}

void DrawSceneOverlayText(
	HWND hwnd,
	const QLearningGrid& world,
	unsigned int windowWidth,
	unsigned int windowHeight,
	const std::wstring& speedLabel,
	bool paused,
	const EpisodeRunUiState& episodeRunUiState) {
	// 小見出し
	// 盤面上の人数ラベルだけは GDI で重ね、部隊図形の上へ読みやすく表示します。
	(void)speedLabel;
	(void)paused;
	(void)episodeRunUiState;

	HDC dc = GetDC(hwnd);
	if (dc == nullptr) {
		return;
	}

	SetBkMode(dc, TRANSPARENT);

	HFONT labelFont = CreateUiFont(15, FW_BOLD);
	HFONT intentFont = CreateUiFont(13, FW_NORMAL);
	const auto oldFont = static_cast<HFONT>(SelectObject(dc, labelFont));
	const SceneLayout layout = BuildLayout(
		static_cast<float>(windowWidth),
		static_cast<float>(windowHeight),
		world.GetGridWidth(),
		world.GetGridHeight());

	for (const BattleUnit& unit : world.GetBattleUnits()) {
		if (!unit.active || unit.count <= 0) {
			continue;
		}

		const RectPx cell = CellRect(layout, world, unit.position.x, unit.position.y);
		RECT textRect = {
			(std::lround(cell.left + 3.0f)),
			(std::lround(cell.top + 3.0f)),
			(std::lround(cell.right - 3.0f)),
			(std::lround(cell.top + 23.0f)),
		};
		std::wstring label = (unit.isGeneral ? L"将" : L"") +
		                     UnitClassShortName(unit.unitClass) + std::to_wstring(unit.count);
		const COLORREF backgroundColor = unit.faction == UnitFaction::Player
			                                 ? (unit.isGeneral ? RGB(0, 80, 160) : RGB(16, 122, 38))
			                                 : (unit.isGeneral ? RGB(160, 40, 20) : RGB(178, 30, 26));
		HBRUSH backgroundBrush = CreateSolidBrush(backgroundColor);
		FillRect(dc, &textRect, backgroundBrush);
		DeleteObject(backgroundBrush);

		RECT shadowRect = textRect;
		OffsetRect(&shadowRect, 1, 1);
		SetTextColor(dc, RGB(4, 8, 10));
		DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &shadowRect,
		          DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		SetTextColor(dc, RGB(255, 255, 255));
		DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		/* AI意図テキストを部隊ラベルの下に表示 */
		std::wstring intentText = AIIntentText(unit.intent);
		if (unit.targetId >= 0) {
			for (const BattleUnit& target : world.GetBattleUnits()) {
				if (target.id == unit.targetId && target.active && target.count > 0) {
					intentText += L"→" + UnitClassShortName(target.unitClass);
					break;
				}
			}
		}
		RECT intentRect = {
			(std::lround(cell.left + 3.0f)),
			(std::lround(cell.top + 26.0f)),
			(std::lround(cell.right - 3.0f)),
			(std::lround(cell.top + 44.0f)),
		};
		SelectObject(dc, intentFont);
		const COLORREF intentColor = unit.faction == UnitFaction::Player
			                             ? RGB(140, 255, 180)
			                             : RGB(255, 180, 140);
		SetTextColor(dc, intentColor);
		DrawTextW(dc, intentText.c_str(), static_cast<int>(intentText.size()), &intentRect,
		          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		SelectObject(dc, labelFont);
	}

	SelectObject(dc, oldFont);
	DeleteObject(intentFont);
	DeleteObject(labelFont);
	ReleaseDC(hwnd, dc);
}

std::wstring BuildWindowTitle(
	const QLearningGrid& world,
	const std::wstring& speedLabel,
	bool paused,
	const EpisodeRunUiState& episodeRunUiState) {
	/* タイトル行組み立て */
	// 現在の合戦状況を 1 行で素早く確認できる文字列にまとめます。
	std::wostringstream stream;
	stream << L"合戦シミュレーション | 合戦 " << world.GetEpisodeCount()
		<< L" | 総ターン " << world.GetTrainingStepCount()
		<< L" | 今回 " << world.GetCurrentEpisodeSteps() << L" ターン"
		<< L" | サイズ " << world.GetGridWidth() << L"x" << world.GetGridHeight()
		<< L" | マップ " << world.GetMapDisplayName()
		<< L" | 速度 " << (paused ? L"一時停止" : speedLabel)
		<< L" | 味方勝率 " << std::fixed << std::setprecision(2) << world.GetRecentSuccessRate()
		<< L" | 敵勝率 " << std::setprecision(2) << world.GetRecentBlockedRate()
		<< L" | 味方 " << world.GetPlayerSoldierCount() << L" 人"
		<< L" | 敵 " << world.GetEnemySoldierCount() << L" 人";

	if (episodeRunUiState.autoRunning && episodeRunUiState.targetEpisode >= 0) {
		stream << L" | 自動 " << episodeRunUiState.targetEpisode << L" 回まで";
	}
	else if (episodeRunUiState.editing) {
		stream << L" | 目標入力 " << FormatEpisodeTargetInput(episodeRunUiState);
	}

	return stream.str();
}

RECT GetEpisodeTargetInputRect(
	const QLearningGrid& world,
	unsigned int windowWidth,
	unsigned int windowHeight) {
	/* 入力欄矩形の公開版 */
	const SceneLayout layout = BuildLayout(
		static_cast<float>(windowWidth),
		static_cast<float>(windowHeight),
		world.GetGridWidth(),
		world.GetGridHeight());
	const RectPx rect = EpisodeTargetInputRectPx(layout);
	return {
		(std::lround(rect.left)),
		(std::lround(rect.top)),
		(std::lround(rect.right)),
		(std::lround(rect.bottom)),
	};
}
