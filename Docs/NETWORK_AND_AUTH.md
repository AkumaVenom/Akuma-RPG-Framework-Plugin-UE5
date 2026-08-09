# Networking, Login and Authority

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

## Server authority

Inventory mutation, currency transactions, quest acceptance/completion, Slayer assignments, building placement, crafting, vendor transactions, battle-pet battle state and similar gameplay mutations are intended to execute on the server.

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
