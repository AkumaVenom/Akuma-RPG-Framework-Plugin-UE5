# Networking, Login and Authority


### Generic Item Use authority (v2.13)

`ARPGItemUseComponent` is replicated and performs all usable-item mutation on the server. Client Inventory UI / Quick Access / Blueprint calls only submit an exact runtime InstanceId. Authority re-resolves that instance from the owned Inventory, checks usability, quantity and item-type cooldown, runs optional custom Blueprint validation/effect logic, applies built-in vital/GAS effects, consumes only after success, and then multicasts cosmetic presentation. Cooldown state is owner-only replicated for local UI.

## Single-player-first account flow

The Account Subsystem provides local profile creation/login using username/password input.

It stores:

- account GUID
- normalized username
- random salt
- password verifier
- registered character GUIDs
- last character GUID

Raw passwords are not stored in character/world save records.

The local verifier exists to separate/protect local profiles. It is not a public-Internet authentication backend.

## Character save binding

After login, the framework can register a character ID to the current account. The persistence component can use the last character ID on the next login/load so the correct character state is restored automatically.

From v2.15.40 this account-character path is **player-character only**. `AARPGAICharacter` shares the RPG component stack for gameplay, but it cannot consume `LastCharacterId` and cannot call the account `SaveCharacter` / `LoadCharacter` path. NPC state is owned by its spawner/world systems. This prevents relogged enemies from aliasing the player save slot and inheriting the player's faction/combat identity.

## Server authority

Inventory mutation, Quick Access assignment/selection/use, equipment switching, currency transactions, quest acceptance/completion, Slayer assignments, building placement, crafting, vendor transactions, battle-pet battle state and similar gameplay mutations are intended to execute on the server.

Quick Access slot layout and active-slot state replicate owner-only. This keeps a player's private hotbar off unrelated clients; other players still receive normal replicated Inventory/Equipment state for visible held gear. Consumable quantity/effects are server authoritative.

World interactions use the player-owned `ARPGInteractionComponent` for RPC routing. This avoids relying on a client to own the vendor/chest/station/NPC actor.

## Direct hosting

`ARPGNetworkSubsystem.HostListenServer` opens a map as a listen server using a chosen port.

`JoinByIP` accepts an IP/hostname and appends the default port when necessary.

Typical LAN use:

`192.168.1.25:7777`

Typical direct Internet use:

`public.ip.address:7777`

The framework does not punch router/firewall holes. Direct public hosting may require port forwarding and firewall rules.

## Production Internet authentication

Do not trust the local Account Subsystem as proof of identity for an untrusted Internet client.

For a public game, authenticate via a dedicated backend or platform provider (for example your own service, Steam/EOS identity, etc.), validate that identity server-side, then map the trusted account ID into the RPG profile/save layer.

## Chat

The supplied PlayerController/GameState pair routes chat server-side. Private/group channels are filtered per receiver rather than placing every private message into one globally replicated array.

The same `FARPGChatMessage` format is used for player and game-generated messages, allowing one UMG chat box to display/filter:

- local/say
- yell
- whisper
- world/zone
- party/raid
- guild/faction
- NPC/boss speech
- system
- quest
- loot
- combat/event messages
