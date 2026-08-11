# Ambient NPC Social Interactions — v2.6.0-alpha

`UARPGAISocialComponent` is an optional, server-authoritative ambient social layer installed on `AARPGAICharacter` as the inherited **AISocial** component. It is disabled by default so all existing NPCs retain their previous movement/combat behaviour until explicitly opted in.

## Intended use

Enable the component on villagers, guards, merchants, workers, travellers, wildlife with social behaviour, or any other NPC archetype that should acknowledge compatible nearby NPCs. The system is deliberately generic: it does not assume every NPC is a villager and it does not hard-code dialogue content.

A typical ambient encounter is:

1. two eligible NPCs enter each other's social detection radius;
2. the initiator receives one randomized opportunity roll;
3. faction, tag, cooldown, combat and shared-interaction checks must all pass;
4. both NPCs are reserved atomically so neither can join a second conversation;
5. wandering/spline travel pauses without destroying the authored route/home state;
6. the initiator approaches while the responder waits;
7. both turn toward one another;
8. the pair interacts for a randomized duration;
9. speaker beats alternate, optionally playing local voice sounds and emitting local FText lines;
10. both NPCs return to their previous ambient movement after cooldown.

Combat, damage and death have priority over social ambience and interrupt the pair immediately.

## Quick setup

On an `AARPGAICharacter`-derived Blueprint:

- select **AISocial**;
- enable **Enable Social Interactions**;
- leave the default `Conversation` interaction entry in the Interaction Pool;
- optionally add animation montages, sounds and lines to that local `Conversation` entry;
- configure faction identity/relationships on the inherited Faction component as usual.

Two NPCs need the same `InteractionId` to use that interaction together. Each NPC plays assets from its own matching entry, so a human villager and a different-skeleton guard can both support `Conversation` while using different animation/sound content.

## Faction safety

Hostile relationships are always rejected in either direction. Social authoring may independently allow:

- same-faction interactions;
- friendly/allied-faction interactions;
- neutral-faction interactions;
- factionless NPCs.

This layer never turns a hostile relationship into friendly ambient chatter. Combat/retaliation remains authoritative.

## Archetype matching

`SocialIdentityTags`, `RequiredPartnerTags` and `BlockedPartnerTags` allow optional designer matching without hard-coded classes. Example identities might include `Social.Villager`, `Social.Guard`, `Social.Merchant` or project-specific tags. Empty requirements allow any otherwise compatible social NPC.

Partner acceptance is symmetric: both participants' required/blocked rules must approve the other actor.

## Interaction pool

Each `FARPGAISocialInteractionDefinition` exposes:

- `InteractionId`;
- relative selection `Weight`;
- Can Initiate / Can Respond;
- Min/Max Duration;
- local Montage choices;
- local Sound choices;
- local FText Lines.

Useful shared ids can include `Conversation`, `Greeting`, `Wave`, `Laugh`, `InspectGoods`, or any project-defined name. Only ids present on both participants are eligible.

## Movement integration

The social component integrates with both existing ambient movement paths:

- Free Roam / Wanderer is paused and restored when appropriate.
- Active Spline routes are manually paused and resumed without discarding route progress.
- The responder stops while the initiator approaches.
- The pair faces one another during the exchange.
- If combat starts while a spline route is socially paused, the social layer clears only its own pause and leaves the spline's combat suspension authoritative.

## Combat priority

An active pair is cancelled if either participant:

- acquires an AI combat target;
- receives a combat hit;
- dies;
- becomes otherwise unavailable for social AI through active attack/defensive/combat state.

This prevents conversations from fighting dodge, stagger, attack, chase or death movement.

## Performance

There is no permanent component Tick. Idle social NPCs use a staggered timer and a local `ECC_Pawn` sphere overlap. Candidate processing is capped by `Max Candidates Per Scan`. Active pairs temporarily use a short update timer for approach/facing/lifecycle, and that timer is removed at interaction end.

Opportunity rolls are throttled independently from scan frequency, and post-interaction plus same-partner cooldowns prevent crowded towns from repeatedly pairing the same actors.

## Multiplayer

Pair selection and lifecycle are server-authoritative. Runtime state (state, partner, interaction id, role and end server time) replicates, while social presentation start/beat/end is multicast to connected clients. Animation/audio/text content remains project-authored.

## Blueprint hooks

Callable/pure API includes:

- `Set Social Interactions Enabled`
- `Try Start Social Interaction With`
- `Force Social Interaction With`
- `Cancel Social Interaction`
- `Is Socially Engaged`
- `Can Socially Interact With`

Events:

- `On Social Interaction Started`
- `On Social Interaction Ended`
- `On Social Line Spoken`
- `On Social State Changed`

`Force Social Interaction With` is particularly useful for PIE testing and scripted/cinematic scenes. It bypasses the random opportunity roll while still honoring safety, faction, tag and shared-interaction compatibility.

## v2.6.1 movement-ownership reliability

Ambient social behaviour no longer toggles `Wanderer.bEnabled`. The Wanderer keeps the spawner/designer's persistent Free-Roam intent while social encounters acquire a temporary `SocialInteraction` movement pause. This makes startup ordering deterministic: a spawner can enable Free Roam while an NPC is already socially reserved, and the NPC resumes that enabled Free-Roam state when the encounter ends. Conversely, disabling Free Roam during a social encounter remains disabled afterward.

Spawner group-cohesion recovery uses a separate temporary pause reason and is considered higher-priority movement ownership for social candidate selection. A recovering follower therefore cannot be pulled into a conversation mid-recovery. If an existing social pair is active, cohesion waits rather than issuing a competing `MoveTo`. Recovery requests are reissued until the follower reaches the true recovery radius, including the hysteresis band between recovery and outer cohesion radii.

Freshly spawned social NPCs also defer their first normal opportunity until the existing Opportunity Retry window has elapsed. This lets Free Roam choose an initial destination before the first ambient conversation without adding another per-NPC timer setting.
