#include "pch.h"
#include "AutoChat.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/CanvasWrapper.h"
using namespace std;

BAKKESMOD_PLUGIN(
    AutoChat,
    "Gameplay Stat Tracker",
    plugin_version,
    PLUGINTYPE_FREEPLAY
)

shared_ptr<CVarManagerWrapper> _globalCvarManager;

// === Stats ===
int jumpCount = 0;
int doubleJumpCount = 0;
int flipCount = 0;
int missCount = 0;

// === State ===
bool wasGroundedLastFrame = true;
bool hasJumpedThisAirborne = false;
bool hasDodgedThisAirborne = false;
ControllerInput lastInput = {};

void AutoChat::onLoad()
{
    _globalCvarManager = cvarManager;
    LOG("Stat Tracker Loaded");

    // Draw overlay
    gameWrapper->RegisterDrawable(
        bind(&AutoChat::RenderStats, this, placeholders::_1)
    );

    // Reset on goal scored
    gameWrapper->HookEvent(
        "Function TAGame.GameEvent_Soccar_TA.EventGoalScored",
        [this](string)
        {
            jumpCount = doubleJumpCount = flipCount = missCount = 0;
            hasJumpedThisAirborne = false;
            hasDodgedThisAirborne = false;
        }
    );

    // Reset on match start (works for both online and freeplay)
    gameWrapper->HookEvent(
        "Function TAGame.GameEvent_TA.Activated",
        [this](string)
        {
            jumpCount = doubleJumpCount = flipCount = missCount = 0;
            hasJumpedThisAirborne = false;
            hasDodgedThisAirborne = false;
            wasGroundedLastFrame = true;
            lastInput = {};
        }
    );
}

void AutoChat::RenderStats(CanvasWrapper canvas)
{
    // Works in both online and freeplay
    if (!gameWrapper->IsInGame())
        return;

    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull())
        return;

    bool isGrounded = car.IsOnGround();
    ControllerInput input = car.GetInput();

    // Jump detection - transition from grounded to airborne
    if (wasGroundedLastFrame && !isGrounded)
    {
        jumpCount++;
        hasJumpedThisAirborne = true;
        hasDodgedThisAirborne = false;
    }

    // Dodge/Flip detection - detect stick input while airborne
    if (!isGrounded && hasJumpedThisAirborne)
    {
        // Check if dodge input (pitch, roll, or yaw) changed from last frame
        bool isDodging = (input.Pitch != 0.0f || input.Roll != 0.0f || input.Yaw != 0.0f);
        bool wasDodging = (lastInput.Pitch != 0.0f || lastInput.Roll != 0.0f || lastInput.Yaw != 0.0f);

        // If dodge input just started
        if (isDodging && !wasDodging && !hasDodgedThisAirborne)
        {
            flipCount++;
            hasDodgedThisAirborne = true;
        }
    }

    // Double jump detection - second jump while still airborne
    if (!isGrounded && hasJumpedThisAirborne && input.Jump && !lastInput.Jump)
    {
        if (!hasDodgedThisAirborne)
        {
            doubleJumpCount++;
        }
        hasJumpedThisAirborne = false;
        hasDodgedThisAirborne = false;
    }

    // Reset on landing
    if (isGrounded && !wasGroundedLastFrame)
    {
        hasJumpedThisAirborne = false;
        hasDodgedThisAirborne = false;
    }

    wasGroundedLastFrame = isGrounded;
    lastInput = input;

    // === Draw UI ===
    int x = 50;
    int y = 50;
    int lineHeight = 25;

    canvas.SetColor({ 255, 255, 255, 255 });
    canvas.DrawString("Stats Tracker", 1.0f, 1.0f, x, y);

    y += lineHeight;
    canvas.DrawString("Jumps: " + to_string(jumpCount), 1.0f, 1.0f, x, y);

    y += lineHeight;
    canvas.DrawString("Double Jumps: " + to_string(doubleJumpCount), 1.0f, 1.0f, x, y);

    y += lineHeight;
    canvas.DrawString("Flips: " + to_string(flipCount), 1.0f, 1.0f, x, y);

    y += lineHeight;
    canvas.DrawString("Missed Shots: " + to_string(missCount), 1.0f, 1.0f, x, y);
}