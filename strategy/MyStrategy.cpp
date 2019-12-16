#include "MyStrategy.hpp"

#include <optional>
#include <map>
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
	#define DEBUG_DRAW(something) debug.draw(something)
#else
	#define DEBUG_DRAW(something)
#endif

class CV2FW final
{
private:
	double m_x;
	double m_y;

public:
	CV2FW(double x, double y)
		: m_x(x), m_y(y)
	{
	}

	operator Vec2Float ()
	{
		return Vec2Float(static_cast<float>(m_x), static_cast<float>(m_y));
	}
};

UnitAction MyStrategy::getAction(Unit const& unit, Game const& game, Debug & debug)
{
	static decltype(unit.id) first_unit = unit.id;

	const auto distance = [&] (double x, double y) {
		return std::abs(unit.position.x - x) + std::abs(unit.position.y - y);
	};

	const auto distance_e = [&] (double x, double y) {
		return std::sqrt((unit.position.x - x) * (unit.position.x - x) + (unit.position.y - y) * (unit.position.y - y));
	};

	const auto distance_e2 = [&](double x, double y) {
		return (unit.position.x - x) * (unit.position.x - x) + (unit.position.y - y) * (unit.position.y - y);
	};

	const auto full_cross = [&] (double x1_left, double x1_right, double y1_top, double y1_bottom, double x2_left, double x2_right, double y2_top, double y2_bottom) {
		return [&] () {
			auto const x1_width = x1_right - x1_left;
			auto const x2_width = x2_right - x2_left;
			if (x1_width < x2_width)
				return (x2_left < x1_left && x1_left < x2_right || x2_left < x1_right && x1_right < x2_right);
			return (x1_left < x2_left && x2_left < x1_right || x1_left < x2_right && x2_right < x1_right);
		}() && [&] () {
			auto const y1_height = y1_top - y1_bottom;
			auto const y2_height = y2_top - y2_bottom;
			if (y1_height < y2_height)
				return (y2_bottom < y1_bottom && y1_bottom < y2_top || y2_bottom < y1_top && y1_top < y2_top);
			return (y1_bottom < y2_bottom && y2_bottom < y1_top || y1_bottom < y2_top && y2_top < y1_top);
		}();
	};

	const auto cross = [&] (double x_left, double x_right, double y_top, double y_bottom) {
		auto const u_x_left = unit.position.x - unit.size.x / 2.0 - 0.5;
		auto const u_x_right = unit.position.x + unit.size.x / 2.0 + 0.5;
		auto const u_y_top = unit.position.y + unit.size.y + 0.5;
		auto const u_y_bottom = unit.position.y - 0.5;
		return full_cross(u_x_left, u_x_right, u_y_top, u_y_bottom, x_left, x_right, y_top, y_bottom);
	};

	const auto nearest_enemy = [&] () {
		auto min_distance = std::numeric_limits<double>::max();
		std::optional<std::pair<std::pair<double, double>, decltype(unit.id)>> result;
		for (auto const& u : game.units)
		{
			if (u.playerId == unit.playerId)
				continue;
			auto const d = distance(u.position.x, u.position.y + game.properties.unitSize.y / 2.0);
			if (d < min_distance)
			{
				min_distance = d;
				result = { { u.position.x, u.position.y + game.properties.unitSize.y / 2.0 }, u.id };
			}
		}
		return result;
	};

	const auto nearest_hp = [&] () {
		auto min_distance = std::numeric_limits<double>::max();
		std::optional<std::pair<double, double>> result;
		auto const e = nearest_enemy();
		for (auto const& l : game.lootBoxes)
		{
			auto const hp = std::dynamic_pointer_cast<const Item::HealthPack>(l.item);
			if (hp == nullptr)
				continue;
			auto d = distance(l.position.x, l.position.y);
			if (e.has_value() && std::abs(e.value().first.first - l.position.x) + std::abs(e.value().first.second - l.position.y) < d)
				d += 100.0;
			if (d < min_distance)
			{
				min_distance = d;
				result = { l.position.x, l.position.y };
			}
		}
		return result;
	};

	constexpr auto best_weapon = WeaponType::PISTOL;

	const auto nearest_weapon = [&] () {
		std::optional<std::pair<double, double>> result;
		if (unit.weapon != nullptr && unit.weapon->typ == best_weapon)
			return result;
		auto min_distance = std::numeric_limits<double>::max();
		for (auto const& l : game.lootBoxes)
		{
			auto const w = std::dynamic_pointer_cast<const Item::Weapon>(l.item);
			if (w == nullptr)
				continue;
			if (unit.weapon != nullptr && w->weaponType != best_weapon)
				continue;
			auto const d = distance(l.position.x, l.position.y);
			if (d < min_distance)
			{
				min_distance = d;
				result = { l.position.x, l.position.y };
			}
		}
		return result;
	};

	const auto get_unit = [&] (decltype(unit.id) id) -> Unit const&
	{
		for (auto const& u : game.units)
			if (u.id == id)
				return u;
		return unit;
	};

	const auto point_of_interest = [&] () -> std::optional<std::pair<double, double>> {
		if ([&] () {
			if (unit.health < game.properties.unitMaxHealth - game.properties.healthPackHealth / 2.0)
				return true;
			for (auto const& u : game.units)
			{
				if (u.playerId == unit.playerId)
					continue;
				if (unit.health < u.health)
					return true;
			}
			return false;
		}())
		{
			auto const hp = nearest_hp();
			if (hp.has_value())
				return hp;
		}
		auto const w = nearest_weapon();
		if (w.has_value())
			return w;
		auto const e = [&] () -> std::optional<std::pair<double, double>> {
			auto const e = nearest_enemy();
			return e.value().first;
			/*if (!e.has_value())
				return std::nullopt;
			auto const& u = get_unit(e.value().second);
			if (u.weapon == nullptr)
				return e.value().first;
			auto const d_e2 = distance_e2(e.value().first.first, e.value().first.second);
			constexpr auto min_range_e2 = 100.0;
			if (min_range_e2 < d_e2)
				return e.value().first;
			if (u.weapon->fireTimer == nullptr)
				return std::nullopt;
			DEBUG_DRAW(CustomData::Log("Enemy fire timer: " + std::to_string(*u.weapon->fireTimer)));
			DEBUG_DRAW(CustomData::Log("Enemy fire rate: " + std::to_string(u.weapon->params.fireRate)));
			if (u.weapon->params.fireRate < *u.weapon->fireTimer)
			{
				DEBUG_DRAW(CustomData::Log("SAFE TO ATTACK!!!"));
				return e.value().first;
			}
			return std::nullopt;*/
		}();
		if (e.has_value())
			return e;
		auto const hp = nearest_hp();
		if (hp.has_value())
			return hp;
		return std::nullopt;
	};

	auto const poi = point_of_interest();

#ifdef _DEBUG
	if (poi.has_value())
	{
		DEBUG_DRAW(CustomData::Line(CV2FW(unit.position.x, unit.position.y), CV2FW(poi.value().first, poi.value().second), static_cast<float>(0.1), ColorFloat(1.0, 1.0, 1.0, 0.5)));
		DEBUG_DRAW(CustomData::Rect(CV2FW(poi.value().first, poi.value().second), CV2FW(0.3, 0.3), ColorFloat(1.0, 1.0, 1.0, 0.5)));
	}
#endif

#ifdef _DEBUG
	if (unit.weapon != nullptr)
		DEBUG_DRAW(CustomData::Log("Spread: " + std::to_string(unit.weapon->spread)));
#endif

	static std::map<decltype(unit.id), std::pair<double, double>> prev_pos;

	UnitAction action;
	action.plantMine = [&] () {
		return false;

		auto const e = nearest_enemy();
		if (!e.has_value())
			return false;
		if (distance(e.value().first.first, e.value().first.second) < game.properties.mineExplosionParams.radius)
			return true;
		return false;
	}();
	action.aim = [&] () {
		static std::map<decltype(unit.id), decltype(action.aim)> prev_aim;
		auto const e = nearest_enemy();
		if (!e.has_value())
			return prev_aim[unit.id];
		auto delta_x = 0.0;
		auto delta_y = 0.0;
		if (unit.weapon != nullptr)
		{
			auto const d = distance_e(e.value().first.first, e.value().first.second);
			auto const t = std::min(d / unit.weapon->params.bullet.speed * game.properties.ticksPerSecond, 5.0);
			delta_x = (e.value().first.first - prev_pos[e.value().second].first) * t;
			delta_y = (e.value().first.second - prev_pos[e.value().second].second) * t;
		}
		prev_aim[unit.id] = Vec2Double(e.value().first.first + delta_x - unit.position.x, e.value().first.second + delta_y - unit.position.y - game.properties.unitSize.y / 2.0);
		DEBUG_DRAW(CustomData::Rect(CV2FW(unit.position.x + prev_aim[unit.id].x, unit.position.y + game.properties.unitSize.y / 2.0 + prev_aim[unit.id].y), CV2FW(0.2, 0.2), ColorFloat(0.0, 1.0, 1.0, 0.5)));
		return prev_aim[unit.id];
	}();
	action.velocity = [&] () {
		if (!poi.has_value())
			return (game.currentTick % 100 < 50 ? -game.properties.unitMaxHorizontalSpeed : game.properties.unitMaxHorizontalSpeed);
		if (poi.value().first < unit.position.x)
			return std::max(-game.properties.unitMaxHorizontalSpeed, (poi.value().first - unit.position.x) * game.properties.ticksPerSecond);
		if (poi.value().first > unit.position.x)
			return std::min(game.properties.unitMaxHorizontalSpeed, (poi.value().first - unit.position.x) * game.properties.ticksPerSecond);
		return 0.0;
	}();
	action.jump = [&] () {
		if (!poi.has_value())
			return true;
		if (poi.value().second > unit.position.y)
			return true;
		if (unit.position.x < poi.value().first - 1.0 && game.level.tiles[static_cast<size_t>(unit.position.x + 1)][static_cast<size_t>(unit.position.y)] == Tile::WALL)
			return true;
		if (unit.position.x > poi.value().first + 1.0 && game.level.tiles[static_cast<size_t>(unit.position.x - 1)][static_cast<size_t>(unit.position.y)] == Tile::WALL)
			return true;
		auto const e = nearest_enemy();
		if (e.has_value())
		{
			DEBUG_DRAW(CustomData::Log("Distance to enemy: " + std::to_string(distance(e.value().first.first, e.value().first.second))));
			if (distance(e.value().first.first, e.value().first.second) < 1.8001)
				return true;
		}
		return false;
	}();
	action.jumpDown = [&] () {
		if (action.jump)
			return false;
		if (!poi.has_value())
			return false;
		if (poi.value().second < unit.position.y)
			return true;
		return false;
	}();
	action.swapWeapon = [&] () {
		if (unit.weapon == nullptr)
			return true;
		if (unit.weapon->typ == best_weapon)
			return false;
		for (auto const& l : game.lootBoxes)
		{
			auto const w = std::dynamic_pointer_cast<const Item::Weapon>(l.item);
			if (w == nullptr)
				continue;
			if (!cross(l.position.x - l.size.x / 2.0, l.position.x + l.size.x / 2.0, l.position.y + l.size.y, l.position.y))
				continue;
			if (w->weaponType == best_weapon)
				return true;
		}
		return false;
	}();
	action.shoot = [&] () {
		auto const e = nearest_enemy();
		if (!e.has_value())
			return false;

		auto const check_aim = [&] (double aim_x, double aim_y) {
			auto current_x = unit.position.x;
			auto current_y = unit.position.y + game.properties.unitSize.y / 2.0;
			auto step_x = aim_x / 1000.0;
			auto step_y = aim_y / 1000.0;
			for (int i = 0; i < 1000; ++i)
			{
				auto const current_x_size = static_cast<size_t>(current_x);
				auto const current_y_size = static_cast<size_t>(current_y);
				if (current_x_size < 0)
					return false;
				if (current_y_size < 0)
					return false;
				if (current_x_size > game.level.tiles.size() - 1)
					return false;
				if (current_y_size > game.level.tiles[0].size() - 1)
					return false;
				if (game.level.tiles[current_x_size][current_y_size] == Tile::WALL)
					return false;
				for (auto const& u : game.units)
				{
					if (u.playerId != unit.playerId)
						continue;
					if (u.id == unit.id)
						continue;
					if (u.position.x - u.size.x / 2.0 <= current_x && current_x <= u.position.x + u.size.x / 2.0)
						return false;
					if (u.position.y <= current_y && current_y <= u.position.y + u.size.y)
						return false;
				}
				current_x += step_x;
				current_y += step_y;
			}
			return true;
		};

		auto const spread = [&] () {
			if (unit.weapon == nullptr)
				return game.properties.weaponParams.at(WeaponType::ASSAULT_RIFLE).minSpread;
			return unit.weapon->spread;
		}();

		auto const aim_down_x = action.aim.x * std::cos(spread) + action.aim.y * std::sin(spread);
		auto const aim_down_y = -action.aim.x * std::sin(spread) + action.aim.y * std::cos(spread);

		auto const aim_up_x = action.aim.x * std::cos(spread) - action.aim.y * std::sin(spread);
		auto const aim_up_y = action.aim.x * std::sin(spread) + action.aim.y * std::cos(spread);

		DEBUG_DRAW(CustomData::Line(CV2FW(unit.position.x, unit.position.y + game.properties.unitSize.y / 2.0), CV2FW(unit.position.x + aim_down_x, unit.position.y + game.properties.unitSize.y / 2.0 + aim_down_y), static_cast<float>(0.1), ColorFloat(1.0, 0.0, 0.0, 0.25)));
		DEBUG_DRAW(CustomData::Line(CV2FW(unit.position.x, unit.position.y + game.properties.unitSize.y / 2.0), CV2FW(unit.position.x + action.aim.x, unit.position.y + game.properties.unitSize.y / 2.0 + action.aim.y), static_cast<float>(0.1), ColorFloat(1.0, 0.0, 0.0, 0.5)));
		DEBUG_DRAW(CustomData::Line(CV2FW(unit.position.x, unit.position.y + game.properties.unitSize.y / 2.0), CV2FW(unit.position.x + aim_up_x, unit.position.y + game.properties.unitSize.y / 2.0 + aim_up_y), static_cast<float>(0.1), ColorFloat(1.0, 0.0, 0.0, 0.25)));

		//if (!check_aim(aim_down_x, aim_down_y))
		//	return false;

		if (!check_aim(action.aim.x, action.aim.y))
			return false;

		//if (!check_aim(aim_up_x, aim_up_y))
		//	return false;

		DEBUG_DRAW(CustomData::Log("SHOOT!"));
		return true;
	}();
	action.reload = [&] () {
		if (action.shoot)
			return false;
		if (unit.weapon == nullptr)
			return false;
		if (unit.weapon->magazine > unit.weapon->params.magazineSize / 2)
			return false;
		DEBUG_DRAW(CustomData::Log("RELOAD!"));
		return true;
	}();
	if (action.jump)
	{
		if (game.level.tiles[static_cast<size_t>(unit.position.x)][static_cast<size_t>(unit.position.y - 0.5)] == Tile::PLATFORM && game.level.tiles[static_cast<size_t>(unit.position.x)][static_cast<size_t>(unit.position.y)] == Tile::EMPTY && unit.jumpState.maxTime < game.properties.unitJumpTime * 0.8)
		{
			DEBUG_DRAW(CustomData::Log("REJUMP! " + std::to_string(unit.jumpState.maxTime)));
			action.jump = false;
		}
	}

	if (first_unit)
	{
		for (auto const& u : game.units)
		{
			if (u.playerId == unit.playerId)
				continue;
			prev_pos[u.id] = { u.position.x, u.position.y + game.properties.unitSize.y / 2.0 };
		}
	}

	return action;
}