#include "Ram.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Settings.h"
#include "Speed.h"


void RamController::Invalidate() noexcept
{
	targetCooldowns.clear();
}


void RamController::Reset() noexcept
{
	targetCooldowns.clear();
}


void RamController::Update() noexcept
{
	auto* player = Player();
	if (!player || player->IsOnMount()) {
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	std::erase_if(targetCooldowns, [now](const auto& a_entry) {
		return (now - a_entry.second) >= std::chrono::milliseconds(750);
	});

	std::vector<RE::ActorHandle> targets;
	if (auto* processLists = RE::ProcessLists::GetSingleton()) {
		targets.reserve(processLists->highActorHandles.size());
		for (const auto& handle : processLists->highActorHandles) {
			const auto actorPtr = handle.get();
			auto*      actor = actorPtr.get();
			if (actor &&
				!actor->IsPlayer() &&
				!actor->IsPlayerRef() &&
				!actor->IsPlayerTeammate() &&
				!actor->IsDeleted() &&
				!actor->IsDead() &&
				std::find(targets.begin(), targets.end(), handle) == targets.end()) {
				targets.push_back(handle);
			}
		}
	}

	for (const auto& handle : targets) {
		const auto actorPtr = handle.get();
		if (auto* actor = actorPtr.get()) {
			TestRange(player, actor);
		}
	}
}


void RamController::TestRam()
{
	const auto* speed = SpeedController::GetSingleton();
	if (speed->GetCurrSpeed() > *Settings::ramSpeedThreshold) {
		Update();
	}
}


void RamController::TestRange(RE::PlayerCharacter* a_player, RE::Actor* a_actor)
{
	if (!a_player || !a_actor || a_actor->IsPlayerTeammate() || a_actor->IsDead() || !a_actor->Is3DLoaded()) {
		return;
	}

	const float dz = a_player->GetPositionZ() - a_actor->GetPositionZ();
	if (dz >= 48.0f || dz <= -96.0f) {
		return;
	}

	const float dx = a_player->GetPositionX() - a_actor->GetPositionX();
	const float dy = a_player->GetPositionY() - a_actor->GetPositionY();
	const float distSq = dx * dx + dy * dy;
	if (distSq > 3600.0f) {
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (const auto it = targetCooldowns.find(a_actor->GetFormID()); it != targetCooldowns.end()) {
		if ((now - it->second) < std::chrono::milliseconds(750)) {
			return;
		}
	}

	if (a_actor->IsAMount()) {
		RE::NiPointer<RE::Actor> rider;
		if (a_actor->GetMountedBy(rider) && rider && (rider->IsPlayerRef() || rider->IsPlayerTeammate())) {
			return;
		}
	}

	if (a_actor->IsGhost() || a_actor->IsInRagdollState()) {
		return;
	}

	const float distance = std::sqrt(distSq);
	if (distance <= 44.0f) {
		ApplyForce(a_player, a_actor, 0.6f);
	} else if (distance <= 57.0f) {
		ApplyForce(a_player, a_actor, 0.3f);
	} else {
		ApplyForce(a_player, a_actor, 0.1f);
	}
}


void RamController::ApplyForce(RE::PlayerCharacter* a_player, RE::Actor* a_target, const float a_mult)
{
	if (!a_player || !a_target) {
		return;
	}

	auto* targetAV = a_target->AsActorValueOwner();
	if (!targetAV) {
		return;
	}

	auto*       speed = SpeedController::GetSingleton();
	const float deltaSpeed = speed->GetCurrSpeed() - *Settings::ramSpeedThreshold;
	if (deltaSpeed <= 0.0f) {
		return;
	}

	const float strength = a_mult * deltaSpeed;
	const float damage = (std::max)(0.0f, strength * *Settings::ramDamage * *Settings::ramDamageMult);
	if (damage <= 0.0f && strength < 0.5f) {
		return;
	}

	targetCooldowns[a_target->GetFormID()] = std::chrono::steady_clock::now();

	speed->ReduceSpeed(*Settings::ramSpeedReduction * deltaSpeed);

	// Position the impulse origin behind and below the target so AIProcess::KnockExplosion
	// lifts NPCs and creatures forward-and-up rather than driving airborne hits into the ground.
	const auto  playerPos = a_player->GetPosition();
	const auto  targetPos = a_target->GetPosition();
	const float rawDx = targetPos.x - playerPos.x;
	const float rawDy = targetPos.y - playerPos.y;
	const float distSq = rawDx * rawDx + rawDy * rawDy;

	float dirX = 0.0f;
	float dirY = 0.0f;
	if (distSq > 1.0e-4f) {
		const float invDist = 1.0f / std::sqrt(distSq);
		dirX = rawDx * invDist;
		dirY = rawDy * invDist;
	} else {
		const float yaw = a_player->GetAngleZ();
		dirX = std::sin(yaw);
		dirY = std::cos(yaw);
	}

	if (auto* targetProcess = a_target->GetActorRuntimeData().currentProcess;
		targetProcess && !a_target->IsInKillMove()) {
		const RE::NiPoint3 impulseOrigin{
			targetPos.x - dirX * 90.0f,
			targetPos.y - dirY * 90.0f,
			targetPos.z - 55.0f
		};
		const float pushForce = std::clamp(12.0f + strength * 0.65f, 12.0f, 45.0f);
		targetProcess->KnockExplosion(a_target, impulseOrigin, pushForce);
	}

	if (damage > 0.0f) {
		targetAV->ModActorValue(
			RE::ACTOR_VALUE_MODIFIER::kDamage,
			RE::ActorValue::kHealth,
			-damage);
	}

	if (!a_target->IsEssential() &&
		!a_target->IsDead() &&
		targetAV->GetActorValue(RE::ActorValue::kHealth) <= 0.0f) {
		a_target->KillImpl(a_player, 0.0f, true, true);
	}
}
