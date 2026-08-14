# Haunted Heists

> **Steal. Survive. Get out.**

**Haunted Heists** is a first-person cooperative horror-heist game built in Unreal Engine 5. Break into haunted properties, steal everything worth carrying, survive the entities protecting the house, and secure the haul before attempting extraction.

The more players disturb the property, the more hostile it becomes. Every stolen object, slammed door, careless sprint, and badly timed flashlight can turn a profitable robbery into a supernatural disaster.

## Development Status

Haunted Heists is currently in **pre-alpha active development**. The present build is an MVP playtest focused on validating the core mission loop, ghost behavior, multiplayer systems, player guidance, and overall horror pacing.

- **Engine:** Unreal Engine 5.5
- **Platform:** Windows PC
- **Players:** Designed for 1–6 players
- **Current phase:** Private MVP playtesting

Expect unfinished visuals, placeholder content, balance changes, and ghosts occasionally discovering exciting new interpretations of physics.

## Core Gameplay Loop

1. Enter the haunted property from the team van.
2. Search rooms for valuables, tools, and the mission's main objective.
3. Manage limited inventory space while avoiding unnecessary noise.
4. Survive paranormal events and ghost haunts as the property becomes more hostile.
5. Deposit carried valuables at the van's right-side deposit point.
6. Review secured items using the inventory station at the back of the van.
7. Secure the main objective and have every surviving player confirm extraction.

Loot that never reaches the van is not secured.

## Key Features

- **Reactive haunting system** — A server-authoritative Director evaluates Panic, house activity, recent targets, and player behavior before selecting scares and haunts.
- **Noise-driven ghost AI** — Ghosts can investigate footsteps, interactions, dropped objects, doors, equipment, and other disturbances.
- **Dynamic ghost behavior** — Haunt ghosts roam, investigate, chase, search last-known locations, interact with doors, and eventually despawn.
- **Panic system** — Paranormal events raise Panic and establish recovery floors. Medicine reduces Panic and lowers its recovery floor.
- **Limited inventory** — Item sizes consume different amounts of inventory space, forcing players to decide what is worth carrying.
- **Hidden loot values** — The exact value of stolen goods remains unknown until the mission concludes.
- **Escalating risk** — Stealing and disturbing the property increases pressure without allowing the house to simply forget what the team has done.
- **Cooperative extraction** — Every surviving player must be ready, all carried valuables must be deposited, and the main objective must be secured before departure.
- **Contextual onboarding** — In-game prompts explain inventory controls, van flow, Panic recovery, objective danger, and extraction blockers.
- **Audio controls** — Independent Master, Music, and SFX volume settings.

## Controls

| Action | Input |
| --- | --- |
| Move | `W`, `A`, `S`, `D` |
| Look | Mouse |
| Interact / Cancel interaction | `E` |
| Sprint | `Left Shift` |
| Crouch | `C` |
| Use held item | `Left Mouse Button` |
| Drop selected item | `G` |
| Cycle inventory | Mouse Wheel |
| Close van inventory | `Tab` |
| Pause | `Esc` |

Jumping is intentionally disabled. The thieves are here to rob a haunted house, not audition for supernatural parkour.

## Panic and Haunts

Panic rises when players experience paranormal activity or become involved in dangerous events. Higher Panic makes a player a more attractive target and can unlock increasingly severe supernatural responses.

Crossing Panic thresholds establishes recovery floors, preventing players from immediately returning to complete safety. Medicine lowers both current Panic and the active recovery floor, creating a limited way to recover during a mission.

Taking the main objective triggers a haunt. Prepare an escape route before grabbing it.

## Van Stations

The van contains separate stations with different purposes:

- **Right side:** Deposit carried valuables and secure them for extraction.
- **Back:** Review the team's stored items and van inventory.
- **Departure point:** Confirm that your player is ready to leave.

If departure fails, the game reports whether the main objective is missing, valuables are still being carried, or another living player has not confirmed.

## Building the Project

### Requirements

- Unreal Engine 5.5
- Visual Studio 2022
- The **Game development with C++** workload
- A compatible Windows SDK

### Setup

1. Clone the repository:

   ```bash
   git clone https://github.com/moesavari/GhostsVsThieves.git
   cd GhostsVsThieves
   ```

2. Right-click `GhostsVsThieves.uproject` and select **Generate Visual Studio project files**.
3. Open the generated solution in Visual Studio.
4. Select **Development Editor** and **Win64**.
5. Build the `GhostsVsThievesEditor` target.
6. Open `GhostsVsThieves.uproject` in Unreal Engine.

After changing reflected C++ headers, close Unreal and perform a normal Visual Studio build instead of relying on Live Coding.

## Repository Structure

| Directory | Purpose |
| --- | --- |
| `Source/` | C++ gameplay systems and framework code |
| `Content/` | Unreal assets, maps, Blueprints, UI, audio, and data |
| `Config/` | Engine, input, gameplay-tag, and project configuration |
| `Build/` | Platform-specific build resources |

Generated directories such as `Binaries`, `DerivedDataCache`, `Intermediate`, `Saved`, and `.vs` should not be committed.

## Media

Gameplay screenshots, development footage, and an official trailer will be added as development progresses.

## Planned Development

- Additional haunted properties and mission layouts
- More ghost models, personalities, and paranormal events
- Expanded tools, valuables, and mission objectives
- Continued multiplayer, accessibility, performance, and balance improvements
- Improved onboarding and in-game visual guidance
- Additional environmental storytelling and house interactions

## Credits

**Created and developed by Moe Savari.**

Haunted Heists uses selected third-party assets, audio, animations, and development tools. Ownership remains with their respective creators. Full attribution is maintained in the in-game Credits screen.

## Feedback and Bug Reports

The game is currently undergoing private playtesting. Reports should include:

- What happened
- What was expected
- Whether the issue occurred in PIE or a packaged build
- Whether the session was solo, host, or client
- Relevant screenshots, video, and log files
- Steps that reliably reproduce the issue

## License

Copyright © 2026 Moe Savari. All rights reserved.

This project is proprietary and is provided for development, portfolio, and authorized evaluation purposes. No permission is granted to copy, redistribute, sell, sublicense, or create derivative works from the source code or original project content without explicit written permission.

Third-party assets remain subject to their original licenses and are not relicensed by this repository.

## Developer

- **Moe Savari**
- [Portfolio](https://moesavari.github.io/portfolio/)
- [GitHub](https://github.com/moesavari)

