#include "pch.h"
#include "AutoChat.h"

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/CarWrapper.h"
#include "bakkesmod/wrappers/BallWrapper.h"
#include "bakkesmod/wrappers/CanvasWrapper.h"

BAKKESMOD_PLUGIN(
    AutoChat,
    "Gameplay Stat Tracker",
    plugin_version,
    PLUGINTYPE_FREEPLAY
)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

// === Stats ===
int jumpCount = 0;
int doubleJumpCount = 0;
int flipCount = 0;
int missCount = 0;

// === State ===
bool wasGroundedLastFrame = true;
bool hasJumpedOnce = false;

void AutoChat::onLoad()
{
    _globalCvarManager = cvarManager;

    LOG("Stat Tracker Loaded");

    // Draw overlay
    gameWrapper->RegisterDrawable(
        std::bind(&AutoChat::RenderStats, this, std::placeholders::_1)
    );

    // Reset on kickoff / goal
    gameWrapper->HookEvent(
        "Function TAGame.GameEvent_Soccar_TA.EventGoalScored",
        [this](std::string)
        {
            jumpCount = doubleJumpCount = flipCount = missCount = 0;
            hasJumpedOnce = false;
        }
    );
}

void AutoChat::RenderStats(CanvasWrapper canvas)
{
    if (!gameWrapper->IsInGame())
        return;

    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull())
        return;

    bool isGrounded = car.IsOnGround();

    // Jump detection
    if (wasGroundedLastFrame && !isGrounded)
    {
        jumpCount++;
        hasJumpedOnce = true;
    }

    // Double jump / flip detection
    if (!isGrounded && hasJumpedOnce && car.HasDoubleJumped())
    {
        doubleJumpCount++;
        hasJumpedOnce = false;
    }

    if (!isGrounded && car.HasFlipped())
    {
        flipCount++;
        hasJumpedOnce = false;
    }

    wasGroundedLastFrame = isGrounded;

    // === Draw UI ===
    canvas.SetPosition({ 50, 50 });
    canvas.SetColor({ 255, 255, 255, 255 });

    canvas.DrawString("Stats Tracker", 1.2f);

    canvas.SetPosition({ 50, 80 });
    canvas.DrawString("Jumps: " + std::to_string(jumpCount));

    canvas.SetPosition({ 50, 105 });
    canvas.DrawString("Double Jumps: " + std::to_string(doubleJumpCount));

    canvas.SetPosition({ 50, 130 });
    canvas.DrawString("Flips: " + std::to_string(flipCount));

    canvas.SetPosition({ 50, 155 });
    canvas.DrawString("Missed Shots: " + std::to_string(missCount));
}
