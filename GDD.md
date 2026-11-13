# KaijuGame: Game Design Document

> Status: v0.3 (movement: sprint, liftoff and fuel thrust replace the boost-meter jump). Items marked **[TUNE]** are starting values to adjust by feel.
> Items marked **[OPEN]** are undecided. Claude should ask before assuming.

## 1. Vision

A colorful, anime-inspired third-person action game where you pilot **Heavy**, a blue hovering
mech armed with lightning weaponry, as part of an elite squad of mech riders defending humanity
from kaiju. You rush through city and forest landscapes fighting swarms of small kaiju while
trying to stop a massive kaiju from destroying city blocks.

**Project goal:** this is also an experiment in how much Claude Code can accomplish in
Unreal Engine 5.7. The focus is on **game feel**: the character controller, hovering,
sprinting, jet jumping and thrusting, dashing, shooting and particle effects.

**Success criteria**
1. A character controller the developer is personally satisfied with.
2. A hectic boss fight: the player rushes in to defend the city from small kaiju while stopping
   a massive kaiju that is destroying city blocks.

## 2. Design Pillars

1. **Feel first.** Movement and shooting must feel responsive, weighty and satisfying before anything else is built.
2. **Scale contrast.** A nimble mech against both small swarms and building-sized kaiju.
3. **Readable chaos.** Lightning, impacts and destruction look spectacular but never hide threats or the crosshair. Kaiju telegraph attacks by lighting up before they strike.
4. **Punishing but fair.** Damage hurts, but every dangerous attack is telegraphed and dodgeable.
5. **Iterate fast.** Gameplay values live in data (UPROPERTY / data assets), not hard-coded, so tuning needs no recompile.

## 3. Setting and Story

- Humanity faces recurring kaiju attacks.
- The player belongs to an elite squad of mech riders. Heavy is the first playable mech.
- Story is a light framing for now. Details **[OPEN]**.

## 4. Art Direction

- **Colorful, anime-inspired** look: strong colors, bold readable silhouettes, stylized lightning and impact effects.
- **Movement inspiration:** the Armored Core series (fast, weighty, boost-driven mech movement).
- **Kaiju inspiration:** Godzilla-style monsters whose bodies **light up before attacks** as the telegraph.
- Reference material for the look: **[OPEN]**.

## 5. Player Character: Heavy

| Attribute | Value |
|---|---|
| Role | Frontline mech |
| Look | Blue armored mech, anime-inspired |
| Theme | Lightning / electricity |
| Movement | Grounded hover, sprint, liftoff jump with fuel thrust, dash evade |
| Weapons | Lightning machine gun, charged shot, lightning sword special |
| Defense | Health bar only (no shield or armor) |

Existing assets: a `_Dev` folder contains the Heavy character with a material and some particles.
Only one playable character for now.

## 6. Camera

- **Third-person chase camera**, set further back to show the mech's size and kaiju scale.
- Spring arm with light camera lag; slight FOV increase while sprinting at speed.
- Aim reticle at screen center when free-aiming; shot direction is derived from the camera.
- In lock-on, the camera frames both Heavy and the target. **[OPEN]**: exact framing.

## 7. Controls

Gamepad and keyboard/mouse are supported equally via **Enhanced Input**
(Input Actions + Mapping Contexts, no hard-coded keys).

| Action | Gamepad | Keyboard / Mouse |
|---|---|---|
| Move | Left stick | WASD |
| Look / aim | Right stick | Mouse |
| Liftoff jump (press) / fuel thrust (hold in air) | A / Cross | Space |
| Sprint (hold) | B / Circle | Left Shift (hold) |
| Dash evade | Tap B / Circle | Tap Left Shift |
| Fire (machine gun) | Right trigger | Left mouse |
| Charged shot | Hold right trigger | Hold left mouse |
| Lightning sword special | **[OPEN]** | **[OPEN]** |
| Lock-on toggle | Right stick click | **[OPEN]** (e.g. Q or middle mouse) |

Bindings are placeholders and can change.

## 8. Movement

### 8.1 Two movement schemes

**Free movement (no lock-on):** Heavy turns to face the direction of the movement input,
relative to the camera.

**Lock-on movement:** when locked to a target, Heavy always faces the target and **strafes
around it**. Movement input moves Heavy relative to the target (left/right orbits, forward/back
closes and opens distance).

Switching between schemes should be smooth. The mech rotates, not snaps.

### 8.2 Grounded hover
- Heavy floats a fixed height above terrain, following slopes smoothly.
- Hover height is found by downward trace; the mech eases toward the target height with a spring/damping feel. **[TUNE]**
- Responsive acceleration with a little inertia and drift on direction changes (Armored Core-style).
- Visual banking into turns and pitching with acceleration.

### 8.3 Sprint
- **Hold** Left Shift / B to sprint: a higher top speed and acceleration while held.
- Sprint is **free**: it uses no fuel or other resource, on the ground or in the air.
- The camera FOV widens slightly once Heavy is actually moving at sprint speed.

### 8.4 Liftoff jump and fuel thrust
- **Liftoff (press Space / A while hovering):** a jet launch that propels Heavy upward and along the
  current movement input (a horizontal push on top of the existing momentum). Liftoff is **free**.
- **Thrust (hold Space / A in the air):** once Heavy stops rising, the thrusters engage and carry it in
  a **gentle climb**, burning **fuel**. Releasing drops Heavy; pressing again in the air resumes thrust
  while fuel remains. Thrust also arrests a fall (e.g. after running off a ledge).
- Air control is limited while falling and somewhat better while thrusting.
- Landing: the hover spring catches Heavy as it nears the ground and returns it to hover height.
  A dust/shockwave effect on landing is planned (movement events already exist for it).

### 8.5 Fuel
- A limited resource that **only thrust** consumes. Liftoff and sprint are free.
- Recharges on its own whenever Heavy is **not thrusting**: **slowly in the air** (falling, rising after
  liftoff, dashing) after a short delay, and **considerably faster once grounded**.
- Shown on the HUD as a bar (turns red when low).

### 8.6 Dash evade
- Triggered by a **quick tap** of Left Shift / B (holding sprints instead). The dash fires on release
  of the tap, so the tap window is kept short.
- A short, locked lunge at high speed in the **movement input direction**, or **backward** (away from
  the camera's facing) with no input. Used to get out of telegraphed attacks.
- The extra speed bleeds off into normal (or sprint) movement afterwards, so the dash flows into a slide.
- **Air dash is enabled for testing** (may be disabled later): it holds altitude during the lunge and is
  limited per airtime. Thrust pauses during a dash.
- **Does not use fuel.** Limited by a short **cooldown**.
- Invulnerability: a short window at the start of the dash is tracked, but nothing uses it until the
  health/damage system exists. **[OPEN]**: whether it should grant full invulnerability.

### 8.7 Starting values **[TUNE]**
| Parameter | Start |
|---|---|
| Max hover speed | 1200 cm/s |
| Acceleration | 2400 cm/s² |
| Sprint speed / acceleration | 2000 cm/s / 3000 cm/s² |
| Sprint FOV increase | +8° |
| Hover height | 120 cm |
| Liftoff vertical speed | 1400 cm/s (≈ 650 cm peak) |
| Liftoff horizontal push | 600 cm/s (max 2400 cm/s horizontal) |
| Air control (falling / thrusting) | 0.4 / 0.6 |
| Thrust climb speed | 250 cm/s |
| Fuel | 100 units, drains 25/s (≈ 4 s of thrust) |
| Fuel recharge | 40/s on the ground (after 0.4 s), 8/s in the air (0.5 s after thrust stops) |
| Dash | 3500 cm/s for 0.2 s (≈ 700 cm + slide), bleeds off at 7000 cm/s² |
| Dash cooldown | 0.6 s from dash start |
| Dash tap window | released within 0.15 s |
| Air dashes per airtime | 1 |
| Dash invulnerability window (unused yet) | 0.15 s |

## 9. Combat

### 9.1 Lightning machine gun (primary)
- Rapid-fire **projectile** lightning bolts (travel time, not hitscan).
- High fire rate, light damage per shot, small spread. **[TUNE]**
- Impact: electric burst VFX, hit marker, subtle camera shake.

### 9.2 Charged shot
- Hold fire to charge; release for a larger, high-damage bolt with an area impact.
- Charge time and damage **[TUNE]**. Piercing or not: **[OPEN]**.

### 9.3 Lightning sword (special)
- A melee special that manifests a glowing blade of lightning, **lightsaber-like**.
- Strong close-range slashes that deal heavy damage; good against small swarms and massive kaiju weak points.
- Activation, duration, cooldown or energy cost **[OPEN]**.
- Needs a mesh/VFX blade and a short attack animation. This is a prime test for Claude's animation work.

### 9.4 Targeting and aim
- **Free aim** with the reticle when not locked on.
- **Lock-on** to a target (see 8.1). Aim assist on gamepad **[OPEN]**.

### 9.5 Damage model
- Player: **health bar only**. Damage should feel **punishing**: a few big hits are dangerous, with clear hit feedback (screen flash, shake, sound).
- Enemies: health values, with optional weak points on massive kaiju **[OPEN]**.

## 10. Enemies

| Type | Role | Prototype behavior |
|---|---|---|
| Small kaiju | Swarming melee threats | Rush at Heavy, attack when close, light up briefly before striking |
| Massive kaiju | Boss-scale threat | Large health pool, stomps, tail sweeps and a ranged blast, each telegraphed by lighting up. Destroys city blocks as it advances |

- The **light-up telegraph** is the central readability mechanic: body glow (emissive) before every attack.
- Placeholders first (simple shapes or a basic mesh). The developer has kaiju models to integrate later.

## 11. Boss Fight Goal

The target experience: Heavy rushes through the city, fighting small kaiju that distract
and swarm, while a massive kaiju stomps through city blocks. The player must balance
protecting the city, killing small kaiju, and dealing enough damage to the big one.
Details such as phases, city-health or timer **[OPEN]**.

## 12. Environments and Destruction

- **City** and **forest** landscapes built from developer-supplied environment assets.
- **Buildings and trees should be destructible** (at a later milestone). Approach **[OPEN]**: Chaos destruction vs. swapping to a broken mesh vs. simple scripted collapse. Start with the simplest approach that looks good.
- Claude should not modify imported environment assets without being asked.

## 13. VFX, Animation and Audio

- **Niagara** for lightning bolts, impact bursts, hover thrusters, boost trails, dash streaks, landing dust and the lightning sword.
- Existing Heavy particles in `_Dev` should be reused where possible.
- **Animation experiment:** the developer wants to test what Claude can do for animation. Likely strong areas: procedural motion (banking, hover bob, recoil in code), Animation Blueprint and state setup, montage triggers. Hand-keyed animation is a harder area, so expect to iterate and check results in the editor.
- Audio: placeholder sounds first. **[OPEN]**

## 14. Technical Approach

- Engine: **Unreal Engine 5.7**. Language: **C++ first**, Blueprints for tuning, visuals and UI.
- Suggested structure (subject to change):
  - `AHeavyCharacter` (pawn, camera, input, lock-on)
  - Custom movement component for hover, boost and dash
  - `ULockOnComponent` (target selection and strafe logic)
  - `AProjectile` (lightning bolt) and a weapon component
  - `UHealthComponent`
  - Kaiju base class with an attack telegraph system (emissive glow + timed attack)
  - Data assets for movement and weapon tuning
- Use Enhanced Input. No direct key bindings in code.
- Single-player only.

## 15. Milestones

1. **Controller prototype:** hover movement, free-movement scheme, camera, input for both devices, on a flat test map using the Heavy asset.
2. **Sprint, jump and dash:** sprint, liftoff, fuel thrust with HUD bar, landing, tap dash (done except landing/dash VFX).
3. **Shooting:** machine gun projectiles, crosshair aiming, impact VFX, then the charged shot.
4. **Lock-on:** target selection and the strafing movement scheme.
5. **Targets and enemies:** dummy target, then a small kaiju with chase, telegraph and attack.
6. **Lightning sword:** blade VFX, attack, animation.
7. **Feel pass:** tune everything, camera shake, FOV, hit feedback, placeholder audio.
8. **Massive kaiju and city:** boss placeholder with telegraphed attacks, environment and destruction.
9. **Boss fight:** put it together into the target experience.

## 16. Out of Scope (for now)

Multiplayer, additional playable characters, story cutscenes, save system, menus, localization.

## 17. Working Agreement for Claude Code

- Read this file and `CLAUDE.md` before large tasks.
- Ask about **[OPEN]** items instead of guessing.
- Put tunable gameplay values in `UPROPERTY(EditAnywhere)` fields or data assets.
- Build with the command in `CLAUDE.md` after each C++ change and fix compile errors.
- Do not touch `Binaries/`, `Intermediate/`, `Saved/` or imported environment assets.
- Treat the Heavy asset in `_Dev` as the source of truth for the character mesh, material and particles.
- Make small, reviewable changes and describe what changed so they can be committed.
