#include "MyStrategy.hpp"

#include <optional>
#include <map>

double distanceSqr(Vec2Double a, Vec2Double b) {
  return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

UnitAction MyStrategy::getAction(Unit const& unit, Game const& game, Debug & debug)
{
	const auto distance = [&] (double x, double y) {
		return std::abs(unit.position.x - x) + std::abs(unit.position.y - y);
	};

	const auto nearest_hp = [&] () {
		auto min_distance = std::numeric_limits<double>::max();
		std::optional<std::pair<double, double>> result;
		for (auto const& l : game.lootBoxes)
		{
			auto const hp = std::dynamic_pointer_cast<const Item::HealthPack>(l.item);
			if (hp == nullptr)
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

	const auto nearest_weapon = [&] () {
		std::optional<std::pair<double, double>> result;
		if (unit.weapon != nullptr && unit.weapon->typ == WeaponType::ASSAULT_RIFLE)
			return result;
		auto min_distance = std::numeric_limits<double>::max();
		for (auto const& l : game.lootBoxes)
		{
			auto const w = std::dynamic_pointer_cast<const Item::Weapon>(l.item);
			if (w == nullptr)
				continue;
			if (unit.weapon != nullptr && w->weaponType != WeaponType::ASSAULT_RIFLE)
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

	const auto point_of_interest = [&] () -> std::optional<std::pair<double, double>> {
		if (unit.health < game.properties.unitMaxHealth / 2)
		{
			auto const hp = nearest_hp();
			if (hp != std::nullopt)
				return hp;
		}
		auto const w = nearest_weapon();
		if (w != std::nullopt)
			return w;
		auto const e = nearest_enemy();
		if (e.has_value() && distance(e.value().first, e.value().second) < 10.0)
			return std::nullopt;
		return e;
	};

	auto const poi = point_of_interest();

	UnitAction action;
	action.plantMine = [&] () {
		auto const e = nearest_enemy();
		if (!e.has_value())
			return false;
		if (distance(e.value().first, e.value().second) < game.properties.mineExplosionParams.radius / 2.0)
			return true;
		return false;
	}();
	action.aim = [&] () {
		static std::map<decltype(unit.id), decltype(action.aim)> prev_aim;
		auto const e = nearest_enemy();
		if (!e.has_value())
			return prev_aim[unit.id];
		prev_aim[unit.id] = Vec2Double(e.value().first - unit.position.x, e.value().second - unit.position.y - game.properties.unitSize.y / 2.0);
		return prev_aim[unit.id];
	}();
	action.velocity = [&] () {
		if (!poi.has_value())
			return (game.currentTick % 20 < 10 ? -game.properties.unitMaxHorizontalSpeed : game.properties.unitMaxHorizontalSpeed);
		if (poi.value().first < unit.position.x)
			return -game.properties.unitMaxHorizontalSpeed;
		if (poi.value().first > unit.position.x)
			return game.properties.unitMaxHorizontalSpeed;
		return 0.0;
	}();
	action.jump = [&] () {
		if (!poi.has_value())
			return true;
		if (poi.value().second > unit.position.y)
			return true;
		if (unit.position.x < poi.value().first && game.level.tiles[static_cast<size_t>(unit.position.x + 1)][static_cast<size_t>(unit.position.y)] == Tile::WALL)
			return true;
		if (unit.position.x > poi.value().first && game.level.tiles[static_cast<size_t>(unit.position.x - 1)][static_cast<size_t>(unit.position.y)] == Tile::WALL)
			return true;
		auto const e = nearest_enemy();
		if (e.has_value() && distance(e.value().first, e.value().second) < 5.0)
			return true;
		return false;
	}();
	action.jumpDown = [&] () {
		if (!poi.has_value())
			return false;
		if (poi.value().second < unit.position.y)
			return true;
		return false;
	}();
	action.swapWeapon = [&] () {
		if (unit.weapon == nullptr)
			return true;
		if (unit.weapon->typ != WeaponType::ASSAULT_RIFLE)
			return true;
		return false;
	}();
	action.reload = false;
	action.shoot = [&] () {
		auto const e = nearest_enemy();
		if (!e.has_value())
			return false;
		auto delta_x = e.value().first - unit.position.x;
		auto delta_y = e.value().second - unit.position.y;
		auto step_x = delta_x / 1000.0;
		auto step_y = delta_y / 1000.0;
		for (double i = 0.0; i < 1000.0; ++i)
			if (game.level.tiles[static_cast<size_t>(unit.position.x + i * step_x)][static_cast<size_t>(unit.position.y + i * step_y)] == Tile::WALL)
				return false;
		return true;
	}();

	/*


  const Unit *nearestEnemy = nullptr;
  for (const Unit &other : game.units) {
    if (other.playerId != unit.playerId) {
      if (nearestEnemy == nullptr ||
          distanceSqr(unit.position, other.position) <
              distanceSqr(unit.position, nearestEnemy->position)) {
        nearestEnemy = &other;
      }
    }
  }
  const LootBox *nearestWeapon = nullptr;
  for (const LootBox &lootBox : game.lootBoxes) {
    if (std::dynamic_pointer_cast<Item::Weapon>(lootBox.item)) {
      if (nearestWeapon == nullptr ||
          distanceSqr(unit.position, lootBox.position) <
              distanceSqr(unit.position, nearestWeapon->position)) {
        nearestWeapon = &lootBox;
      }
    }
  }
  Vec2Double targetPos = unit.position;
  if (unit.weapon == nullptr && nearestWeapon != nullptr) {
    targetPos = nearestWeapon->position;
  } else if (nearestEnemy != nullptr) {
    targetPos = nearestEnemy->position;
  }
  debug.draw(CustomData::Log(
      std::string("Target pos: ") + targetPos.toString()));
  Vec2Double aim = Vec2Double(0, 0);
  if (nearestEnemy != nullptr) {
    aim = Vec2Double(nearestEnemy->position.x - unit.position.x,
                     nearestEnemy->position.y - unit.position.y);
  }
  bool jump = targetPos.y > unit.position.y;
  if (targetPos.x > unit.position.x &&
      game.level.tiles[size_t(unit.position.x + 1)][size_t(unit.position.y)] ==
          Tile::WALL) {
    jump = true;
  }
  if (targetPos.x < unit.position.x &&
      game.level.tiles[size_t(unit.position.x - 1)][size_t(unit.position.y)] ==
          Tile::WALL) {
    jump = true;
  }
  UnitAction action;
  action.velocity = targetPos.x - unit.position.x;
  action.jump = jump;
  action.jumpDown = !action.jump;
  action.aim = aim;
  action.shoot = true;
  action.reload = false;
  action.swapWeapon = false;
  action.plantMine = false;
  */


	return action;
}