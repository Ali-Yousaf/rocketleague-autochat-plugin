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
int flipCount = 0;
int airborneTime = 0;
int supersonicTime = 0;
int wallTouchCount = 0;
int ballTouchCount = 0;
int airTouchCount = 0;
int demoCount = 0;
int missCount = 0;
float totalDistance = 0.0f;
Vector lastCarPos = { 0, 0, 0 };

// === State ===
bool wasGroundedLastFrame = true;
bool hasJumpedThisAirborne = false;
bool hasDodgedThisAirborne = false;
bool wasAirborneLastFrame = false;
bool wasSupersonicLastFrame = false;
bool wasOnWallLastFrame = false;
int ballTouchDebounce = 0; // Debounce counter to prevent double counting
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
    //gameWrapper->HookEvent(
    //    "Function TAGame.GameEvent_Soccar_TA.EventGoalScored",
    //    [this](string)
    //    {
    //        jumpCount = flipCount = 0;
    //        airborneTime = supersonicTime = wallTouchCount = 0;
    //        ballTouchCount = airTouchCount = demoCount = 0;
    //        totalDistance = 0.0f;
    //        hasJumpedThisAirborne = false;
    //        hasDodgedThisAirborne = false;
    //        ballTouchDebounce = 0;
    //    }
    //);

    // Reset on match start
    gameWrapper->HookEvent(
        "Function TAGame.GameEvent_TA.Activated",
        [this](string)
        {
            jumpCount = flipCount = 0;
            airborneTime = supersonicTime = wallTouchCount = 0;
            ballTouchCount = airTouchCount = demoCount = 0;
            totalDistance = 0.0f;
            hasJumpedThisAirborne = false;
            hasDodgedThisAirborne = false;
            wasGroundedLastFrame = true;
            wasAirborneLastFrame = false;
            wasSupersonicLastFrame = false;
            wasOnWallLastFrame = false;
            ballTouchDebounce = 0;
            lastInput = {};
        }
    );

    // Miss count
    gameWrapper->HookEvent(
        "Function TAGame.Ball_TA.EventHitGoal",
        [this](string)
        {
            missCount++;
        }
    );

    // Track ball touches with debounce
    gameWrapper->HookEvent(
        "Function TAGame.Car_TA.OnHitBall",
        [this](string)
        {
            // Only count if debounce is 0 (prevents counting same touch twice)
            if (ballTouchDebounce == 0)
            {
                ballTouchCount++;

                // Check if airborne and count as air touch
                CarWrapper car = gameWrapper->GetLocalCar();
                if (!car.IsNull() && !car.IsOnGround())
                {
                    airTouchCount++;
                }

                ballTouchDebounce = 5; // Set debounce for next 5 frames
            }
        }
    );

    // Track demolitions
    gameWrapper->HookEvent(
        "Function TAGame.Car_TA.Demolish",
        [this](string)
        {
            demoCount++;
        }
    );
}

void AutoChat::RenderStats(CanvasWrapper canvas)
{
    if (!gameWrapper->IsInGame() && !gameWrapper->GetCurrentGameState())
        return;

    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull())
        return;

    bool isGrounded = car.IsOnGround();
    ControllerInput input = car.GetInput();
    Vector carVelocity = car.GetVelocity();
    Vector carPos = car.GetLocation();
    float currentSpeed = carVelocity.magnitude();
    bool isSuperSonic = currentSpeed > 2200.0f; // Supersonic threshold

    // Decrement debounce counter
    if (ballTouchDebounce > 0)
        ballTouchDebounce--;

    // Calculate distance traveled
    if (lastCarPos.X != 0 || lastCarPos.Y != 0 || lastCarPos.Z != 0)
    {
        Vector posDelta = carPos - lastCarPos;
        totalDistance += posDelta.magnitude();
    }
    lastCarPos = carPos;

    // Supersonic time tracking
    if (isSuperSonic && wasSupersonicLastFrame)
        supersonicTime++;

    // Airborne time tracking
    if (!isGrounded && wasAirborneLastFrame)
        airborneTime++;

    // Jump detection
    if (wasGroundedLastFrame && !isGrounded)
    {
        jumpCount++;
        hasJumpedThisAirborne = true;
        hasDodgedThisAirborne = false;
    }

    // Dodge/Flip detection
    if (!isGrounded && hasJumpedThisAirborne)
    {
        bool isDodging = (input.Pitch != 0.0f || input.Roll != 0.0f || input.Yaw != 0.0f);
        bool wasDodging = (lastInput.Pitch != 0.0f || lastInput.Roll != 0.0f || lastInput.Yaw != 0.0f);

        if (isDodging && !wasDodging && !hasDodgedThisAirborne)
        {
            flipCount++;
            hasDodgedThisAirborne = true;
        }
    }

    // Reset on landing
    if (isGrounded && !wasGroundedLastFrame)
    {
        hasJumpedThisAirborne = false;
        hasDodgedThisAirborne = false;
    }

    // Wall touch detection - check if car is on a wall
    bool isOnWall = !isGrounded && (abs(carPos.Y) > 4900.0f || abs(carPos.X) > 8100.0f);
    if (isOnWall && !wasOnWallLastFrame)
    {
        wallTouchCount++;
    }

    wasGroundedLastFrame = isGrounded;
    wasAirborneLastFrame = !isGrounded;
    wasSupersonicLastFrame = isSuperSonic;
    wasOnWallLastFrame = isOnWall;
    lastInput = input;

    // === Draw UI ===
    int x = 20;
    int y = 20;
    int lineHeight = 28;
    float titleSize = 1.4f;
    float sectionSize = 1.0f;
    float statSize = 0.95f;

    // Title
    canvas.SetColor({ 100, 200, 255, 255 });
    canvas.DrawString("=== STAT TRACKER ===", titleSize, titleSize, x, y);
    y += lineHeight + 3;

    // Movement Stats (Yellow)
    canvas.SetColor({ 255, 200, 50, 255 });
    canvas.DrawString("MOVEMENT", sectionSize, sectionSize, x, y);
    y += lineHeight - 5;

    canvas.SetColor({ 255, 255, 255, 255 });
    canvas.DrawString("Jumps: " + to_string(jumpCount) + " | Flips: " + to_string(flipCount), statSize, statSize, x, y);
    y += lineHeight - 5;

    canvas.DrawString("Wall Touches: " + to_string(wallTouchCount) + " | Air Time: " + to_string(airborneTime / 60) + "s", statSize, statSize, x, y);
    y += lineHeight + 3;

    // Performance Stats (Green)
    canvas.SetColor({ 100, 255, 150, 255 });
    canvas.DrawString("PERFORMANCE", sectionSize, sectionSize, x, y);
    y += lineHeight - 5;

    canvas.SetColor({ 255, 255, 255, 255 });
    canvas.DrawString("Current Speed: " + to_string((int)(currentSpeed / 16)) + " KPH", statSize, statSize, x, y);
    y += lineHeight - 5;

    canvas.DrawString("Supersonic Time: " + to_string(supersonicTime / 60) + "s", statSize, statSize, x, y);
    y += lineHeight - 5;

    canvas.DrawString("Distance: " + to_string((int)(totalDistance / 1000)) + "km", statSize, statSize, x, y);
    y += lineHeight - 5;

    canvas.DrawString("Lendis so far: " + to_string((int)(missCount)), statSize, statSize, x, y);
    y += lineHeight + 3;

    // Ball Interaction (Cyan)
    canvas.SetColor({ 100, 200, 255, 255 });
    canvas.DrawString("BALL INTERACTION", sectionSize, sectionSize, x, y);
    y += lineHeight - 5;

    canvas.SetColor({ 255, 255, 255, 255 });
    canvas.DrawString("Ball Touches: " + to_string(ballTouchCount) + " | Air Touches: " + to_string(airTouchCount), statSize, statSize, x, y);
    y += lineHeight + 3;

    // Combat Stats (Red)
    canvas.SetColor({ 255, 100, 100, 255 });
    canvas.DrawString("COMBAT", sectionSize, sectionSize, x, y);
    y += lineHeight - 5;

    canvas.SetColor({ 255, 255, 255, 255 });
    canvas.DrawString("Demolitions: " + to_string(demoCount), statSize, statSize, x, y);
}