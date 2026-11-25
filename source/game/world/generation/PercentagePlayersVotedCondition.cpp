#include "game/world/generation/PercentagePlayersVotedCondition.hpp"
#include "game/core/MasterGameMode.hpp"

PercentagePlayersVotedCondition::PercentagePlayersVotedCondition(const XmlObject& object) : IntegerCondition(object)
{
}

PercentagePlayersVotedCondition::~PercentagePlayersVotedCondition()
{
}

int PercentagePlayersVotedCondition::GetValue(MasterGameMode* gameMode) const
{
    return gameMode != NULL ? gameMode->GetBuildOffVotingPercent() : 0;
}
