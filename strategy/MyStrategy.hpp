#ifndef _MY_STRATEGY_HPP_
#define _MY_STRATEGY_HPP_

#include "Debug.hpp"
#include "model/CustomData.hpp"
#include "model/Game.hpp"
#include "model/Unit.hpp"
#include "model/UnitAction.hpp"

class MyStrategy
{
public:
  MyStrategy() = default;
  UnitAction getAction(Unit const& unit, Game const& game, Debug & debug);
};

#endif