#include "MyStrategy.hpp"

#include <optional>
#include <map>
#include <algorithm>
#include <cmath>

UnitAction MyStrategy::getAction(Unit const& unit, Game const& game, Debug & debug)
{
	const auto distance = [&] (double x, double y) {
		return std::abs(unit.position.x - x) + std::abs(unit.position.y - y);
	};

	const auto cross = [&] (double x_left, double x_right, double y_top, double y_bottom) {
		auto const u_x_left = unit.position.x - unit.size.x / 2.0 - 0.5;
		auto const u_x_right = unit.position.x + unit.size.x / 2.0 + 0.5;
		auto const u_y_top = unit.position.y + unit.size.y + 0.5;
		auto const u_y_bottom = unit.position.y - 0.5;
		return ((u_x_left < x_left && x_left < u_x_right || u_x_left < x_right && x_right < u_x_right) && (u_y_bottom < y_bottom && y_bottom < u_y_top || u_y_bottom < y_top && y_top < u_y_top));
	};

	const auto nearest_enemy = [&] () {
		auto min_distance = std::numeric_limits<double>::max();
		std::optional<std::pair<double, double>> result;
		for (auto const& u : game.units)
		{
			if (u.playerId == unit.playerId)
				continue;
			auto const d = distance(u.position.x, u.position.y + game.properties.unitSize.y / 2.0);
			if (d < min_distance)
			{
				min_distance = d;
				result = { u.position.x, u.position.y + game.properties.unitSize.y / 2.0 };
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
			if (e.has_value() && std::abs(e.value().first - l.position.x) + std::abs(e.value().second - l.position.y) < d)
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
			if (hp != std::nullopt)
				return hp;
		}
		auto const w = nearest_weapon();
		if (w != std::nullopt)
			return w;
		auto const e = nearest_enemy();
		//if (e.has_value() && distance(e.value().first, e.value().second) < 10.0)
		//	return std::nullopt;
		return e;
	};

	auto const poi = point_of_interest();

	if (poi.has_value())
	{
		debug.draw(CustomData::Line(Vec2Float(unit.position.x, unit.position.y), Vec2Float(poi.value().first, poi.value().second), 0.1, ColorFloat(1.0, 1.0, 1.0, 0.5)));
		debug.draw(CustomData::Rect(Vec2Float(poi.value().first, poi.value().second), Vec2Float(0.3, 0.3), ColorFloat(1.0, 1.0, 1.0, 0.5)));
	}

	if (unit.weapon != nullptr)
		debug.draw(CustomData::Log("Spread: " + std::to_string(unit.weapon->spread)));

	UnitAction action;
	action.plantMine = [&] () {
		return false;

		auto const e = nearest_enemy();
		if (!e.has_value())
			return false;
		if (distance(e.value().first, e.value().second) < game.properties.mineExplosionParams.radius)
			return true;
		return false;
	}();
	action.aim = [&] () {
		static std::map<decltype(unit.id), decltype(action.aim)> prev_aim;
		static std::pair<double, double> prev_pos;
		auto const e = nearest_enemy();
		if (!e.has_value())
			return prev_aim[unit.id];
		auto delta_x = 0.0;
		auto delta_y = 0.0;
		if (unit.weapon != nullptr)
		{
			auto const d = std::sqrt((unit.position.x - e.value().first) * (unit.position.x - e.value().first) + (unit.position.y - e.value().second) * (unit.position.y - e.value().second));
			auto const t = std::min(d / unit.weapon->params.bullet.speed * game.properties.ticksPerSecond, 10.0);
			delta_x = (e.value().first - prev_pos.first) * t;
			delta_y = (e.value().second - prev_pos.second) * t;
		}
		prev_pos = e.value();
		prev_aim[unit.id] = Vec2Double(e.value().first + delta_x - unit.position.x, e.value().second + delta_y - unit.position.y - game.properties.unitSize.y / 2.0);
		debug.draw(CustomData::Rect(Vec2Float(unit.position.x + prev_aim[unit.id].x, unit.position.y + game.properties.unitSize.y / 2.0 + prev_aim[unit.id].y), Vec2Float(0.2, 0.2), ColorFloat(0.0, 1.0, 1.0, 0.5)));
		return prev_aim[unit.id];
	}();
	action.velocity = [&] () {
		if (!poi.has_value())
			return (game.currentTick % 20 < 10 ? -game.properties.unitMaxHorizontalSpeed : game.properties.unitMaxHorizontalSpeed);
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
			debug.draw(CustomData::Log("Distance to enemy: " + std::to_string(distance(e.value().first, e.value().second))));
			if (distance(e.value().first, e.value().second) < 1.8001)
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
		auto delta_x = action.aim.x;
		auto delta_y = action.aim.y;
		auto step_x = delta_x / 1000.0;
		auto step_y = delta_y / 1000.0;
		for (double i = 0.0; i < 1000.0; ++i)
			if (game.level.tiles[static_cast<size_t>(unit.position.x + i * step_x)][static_cast<size_t>(unit.position.y + game.properties.unitSize.y / 2.0 + i * step_y)] == Tile::WALL)
				return false;
		debug.draw(CustomData::Log("SHOOT!"));
		return true;
	}();
	action.reload = [&] () {
		if (action.shoot)
			return false;
		if (unit.weapon == nullptr)
			return false;
		if (unit.weapon->magazine > unit.weapon->params.magazineSize / 2)
			return false;
		debug.draw(CustomData::Log("RELOAD!"));
		return true;
	}();
	if (action.jump)
	{
		if (game.level.tiles[static_cast<size_t>(unit.position.x)][static_cast<size_t>(unit.position.y - 0.5)] == Tile::PLATFORM && game.level.tiles[static_cast<size_t>(unit.position.x)][static_cast<size_t>(unit.position.y)] == Tile::EMPTY && unit.jumpState.maxTime < game.properties.unitJumpTime * 0.8)
		{
			debug.draw(CustomData::Log("REJUMP! " + std::to_string(unit.jumpState.maxTime)));
			action.jump = false;
		}
	}
	return action;
}