> **v2.18.5:** Persistent world state is now account-scoped automatically: Single Player uses the logged-in account, Host & Play uses the host account as the one shared authoritative world, and joined clients never own a local world save. World save is v10 and validates its embedded account scope. Existing v9 shared world slots are not auto-imported.

> **v2.18.4:** Frontend local travel now requires **Default Gameplay GameMode** and explicitly forces that `ARPGGameMode` child through Unreal `game=` URL options for Single Player/Host & Play. This supersedes runtime-only GameMode guessing; v2.18.3 recovery remains a secondary fail-safe.

# Frontend Login, Main Menu and Direct-IP Networking

## Scope

v2.18.3 retains the v2.18 native, Blueprint-reskinnable frontend for the framework's **local profile + direct-IP** workflow, includes the v2.18.1 UE5.8 compile-signature corrections, retains the v2.18.2 frontend-to-gameplay input/identity handoff fix, and adds destination-authored GameMode recovery for the real PIE case where the gameplay map loaded under `ARPGFrontendGameMode`:

`Blank Main Menu Map -> Login/Create Account -> Single Player / Host & Play / Join by IP -> Server-approved profile identity -> Gameplay pawn -> Character persistence`

The supplied UI is a functional native fallback. Projects can derive Widget Blueprints from the native widget classes and replace the presentation without replacing account, travel or server-authority logic.

> **Security boundary:** the local username/password system protects and separates SaveGame profiles on the local machine. It is an account-selection/login gate, **not SaveGame encryption and not Internet-grade authentication**. Passwords are never sent to a listen host. A direct-IP host receives a locally verified AccountId/username/CharacterId claim and treats it as a trusted direct-IP identity. A public/untrusted Internet game must authenticate through a backend or platform identity provider and map that trusted identity into this framework layer.

---

## 1. Create the blank Main Menu level

The plugin cannot manufacture a project `.umap` from runtime C++, so create one project asset:

1. **File -> New Level -> Empty Level**.
2. Save it as, for example, `MainMenu`.
3. Open **World Settings**.
4. Set **GameMode Override** to `ARPGFrontendGameMode` or a Blueprint child of it.

`ARPGFrontendGameMode` intentionally has no gameplay pawn. Its `ARPGFrontendPlayerController` owns the frontend UI, mouse cursor and UI-only input mode.


### Required deterministic gameplay GameMode setting — v2.18.4

In **Project Settings -> Game -> Akuma's RPG Framework -> Frontend**, set **Default Gameplay GameMode** to the same project Blueprint used by the gameplay map (for example `ARPG_GameMode`, parent `ARPGGameMode`). This is intentionally explicit. Single Player and Host & Play validate the class and travel with `game=<resolved class path>`, which has higher precedence than map/project defaults. If the setting is empty or invalid, local gameplay travel is refused and the native Main Menu shows the exact error. Direct-IP joining does not send a client-selected GameMode; the remote host owns that choice.

### v2.18.3 destination GameMode fail-safe

The gameplay map must still author its normal `ARPGGameMode`/project child in World Settings. During travel, if UE5.8 PIE nevertheless instantiates `ARPGFrontendGameMode` on that already-loaded destination, the frontend mode reads the destination world’s own `DefaultGameMode` and performs one guarded absolute reopen with that authored class explicitly forced. This is automatic and does not require a Level Blueprint travel workaround. A recovery marker prevents infinite loops.

For a normal packaged project also set:

**Project Settings -> Project -> Maps & Modes -> Game Default Map = MainMenu**

The Editor Startup Map can remain your development map if preferred.

---

## 2. Configure the global frontend/network defaults

Open:

**Project Settings -> Game -> Akuma's RPG Framework**

### Frontend

- **Default Main Menu Map** — e.g. `MainMenu`.
- **Default Gameplay Map** — your actual RPG map, e.g. `StartingIslandMap`.
- **Return To Main Menu On Network Failure** — recommended `true`.

### Networking

- **Default Listen Port** — `7777` by default.
- **Max Players** — default `8`; copied into the authoritative GameSession.
- **Default LAN Listen Server** — recommended `true` for LAN testing.
- **Profile Handshake Timeout Seconds** — default `15`.
- **Require Local Profile For Gameplay** — recommended `true` for the new frontend flow.

`Require Local Profile For Gameplay` can be disabled for a legacy Guest/no-login project, but the v2.18 frontend is designed around an authenticated local profile before travel.

---

## 3. Native Login UI

The frontend controller uses `ARPGLoginWidget` by default.

It supplies:

- Username field
- Password field with password masking
- **Login**
- **Create Account**
- Status/error feedback
- **Quit**

Username rules are 3-32 characters. Password input is 4-128 characters. The widget clears the password field after a login/create attempt.

### Account creation transaction

Creating an account writes:

- one account record in `ARPG_LocalAccounts`
- generated AccountId
- normalized username
- random salt
- local password verifier
- creation time
- a dedicated non-secret account profile save: `ARPG_Account_<AccountId>`

If the account profile save cannot be established, account creation is rolled back rather than leaving an unusable reserved username.

The account profile stores non-secret frontend preferences such as last join address, port, LAN setting and gameplay map. **It does not contain the raw password, verifier or salt.**

`Create Account` in the native frontend immediately logs into the new profile after successful creation and opens the Main Menu screen.

v2.18.2 also creates a stable `LastCharacterId` as part of the account transaction. Existing accounts with no character identity are repaired on successful login. This means **Single Player / Host / Join always leave the frontend with a persistent AccountId + CharacterId pair already established**, before any gameplay pawn can BeginPlay.

---

## 4. Native Main Menu UI

After login, `ARPGMainMenuWidget` provides:

- current logged-in username
- **Single Player**
- **Host & Play**
- listen port field
- LAN toggle
- direct-IP / hostname field
- **Join By IP**
- **Logout**
- **Quit**
- travel/network error status

The last port/LAN/join-address preferences are restored from the account profile.

### Single Player

`Single Player` opens the configured Gameplay Map without `listen`.

The local player's approved AccountId/CharacterId becomes the character persistence namespace. The v2.17.3 fresh/load/load-failed bootstrap remains unchanged after identity binding.

### Host & Play

`Host & Play` opens the Gameplay Map as an Unreal listen server:

`GameplayMap?listen?Port=7777?ARPG_LAN=1`

The host is simultaneously the authoritative server and a local player. World saving remains host/server-owned. In v2.18.5 that authoritative world is keyed by the host account: `ARPG_<HostAccountId>_World_<WorldId>`. Every remote player shares that host world while retaining a separate account/character save.

### Join By IP

Accepted examples:

- `192.168.1.25`
- `192.168.1.25:7777`
- `my-host-name:7777`
- `[IPv6-address]:7777`

When no port is supplied, the configured default port is appended. Travel-option characters such as `?`, `/`, `\\` and `#` are rejected from the typed address rather than being allowed to inject arbitrary travel options.

The framework listens for Unreal network/travel failures and can automatically return to the configured Main Menu map with the last failure message retained for UI display.

---

## 5. Reskinning both frontend screens

No custom UMG is required for functionality. For project art, derive Widget Blueprints from:

- `ARPGLoginWidget`
- `ARPGMainMenuWidget`

Then create a Blueprint child of `ARPGFrontendPlayerController` and assign:

- **Login Widget Class**
- **Main Menu Widget Class**

Finally use a Blueprint child of `ARPGFrontendGameMode` whose PlayerController Class is that customized frontend controller.

### Login binding names

A custom Login Widget may provide widgets with these names:

- `TitleText`
- `SubtitleText`
- `UsernameInput`
- `PasswordInput`
- `StatusText`
- `LoginButton`
- `CreateAccountButton`
- `QuitButton`

Blueprint presentation hooks:

- `On ARPG Login Screen Refreshed`
- `On ARPG Login Status Changed`

### Main Menu binding names

- `TitleText`
- `AccountText`
- `StatusText`
- `SinglePlayerButton`
- `HostAndPlayButton`
- `ListenPortInput`
- `LANCheckBox`
- `JoinAddressInput`
- `JoinByIPButton`
- `LogoutButton`
- `QuitButton`

Blueprint presentation hooks:

- `On ARPG Main Menu Refreshed`
- `On ARPG Main Menu Status Changed`

The gameplay/network logic does not depend on the native dark/gold fallback style.

---

## 6. Gameplay profile handshake

The major v2.18 networking change is that a multiplayer gameplay pawn is **not allowed to start persistence before its connection identity is known**.

### Local host / single player

The authoritative local PlayerController reads the already password-verified local Account Subsystem and binds:

- AccountId
- username
- existing LastCharacterId, or a new CharacterId

### Remote direct-IP player

The server keeps the remote player at the PlayerController stage and requests profile identity. The remote client sends only:

- AccountId
- normalized/display username
- LastCharacterId, when one exists

**Password, salt and password verifier are never RPC parameters.**

The listen server then:

1. validates the submitted identity shape;
2. rejects a duplicate active AccountId;
3. rejects a duplicate active CharacterId;
4. generates a CharacterId if this is the account's first character;
5. permanently binds that identity to the connection for the session;
6. notifies the client of the approved CharacterId;
7. only then starts the gameplay pawn.

The client stores the server-approved CharacterId in its already verified local account index so a later reconnect presents the same stable identity.

The handshake has a configurable timeout. A rejected/timed-out client never receives a gameplay pawn.

---

## 6B. Account-scoped world persistence (v2.18.5)

World-owned state is intentionally separate from character-owned state. Buildings, runtime storage/production, settlement residents and dungeon/world progression are stored in the authoritative **world save**.

- **Logged-in Single Player:** `ARPG_<AccountId>_World_<WorldId>`
- **Host & Play:** the listen host AccountId owns one shared authoritative world slot. Remote clients contribute replicated/server-authoritative changes to that same host world.
- **Joined client:** never loads or writes a local world save.
- **Guest/direct gameplay without login or dedicated server:** retains `ARPG_World_<WorldId>` for backward compatibility.

`ARPGGameMode` captures the active world scope once at BeginPlay and exposes `Active World Save World Id`, `Active World Save Account Id`, and `Active World Save Slot Name` in runtime Details. World save v10 embeds `ScopeAccountId`; a mismatched v10 payload is rejected on load.

**Migration:** v2.18.4 and older used one shared v9 slot for all local accounts. v2.18.5 deliberately does not auto-assign that mixed slot to the first account that logs in. Existing account character saves are unaffected; each logged-in account creates/loads its own v10 world from this release onward.

## 7. Multiplayer character save ownership

### Why this changed

Before v2.18, `MakeCharacterSlotName(CharacterId)` was based on the process-wide local Account Subsystem. On a listen server that is the **host's** local account, which is not a valid namespace for a remote player's authoritative save.

v2.18 adds connection-aware paths:

- `Make Character Slot Name For Account`
- `Make Character Slot Name For Actor`
- `Resolve Character Account Id`
- `Does Character Save Exist For Actor`

A player-controlled character first resolves the server-approved `ARPGPlayerController.AccountId`. The host GameInstance account is a fallback only for the locally controlled host/standalone player.

Character slots therefore look like:

`ARPG_<AccountId>_Char_<CharacterId>`

and two connected players cannot write the same namespace merely because they are running inside one listen-server process.

### Host-authoritative direct-IP persistence

For joined clients, the authoritative character snapshot lives on the **host machine** under that remote AccountId + CharacterId. This is deliberate: Inventory, Quick Access, equipment, progression and other gameplay mutation are server-authoritative while connected.

The remote client's local account database stores the identity mapping, but it does not secretly overwrite authoritative runtime state from the client.

### Manual `SaveNow` from a joined client

A project UI may continue calling the character Persistence component's existing `SaveNow` node. In standalone/listen-host play it commits locally on authority. On a joined client, v2.18 routes that request through the player's owned replicated Persistence component to the host; the host writes the accepted AccountId + CharacterId slot and returns the authoritative result through `OnManualCharacterSaveResult`. The client never writes the host snapshot directly.

Consequences:

- reconnecting the same local profile to the **same host** restores that host's character snapshot;
- joining a **different host machine** with the same account identity can produce a fresh host-side character because that other host does not possess the first host's save files;
- portable characters across unrelated hosts require a trusted backend/cloud persistence service. The framework does not fake that by trusting an arbitrary client-supplied Inventory blob.

This is the correct authority boundary for trusted direct-IP play.

---

## 8. What remains automatic in multiplayer

The frontend does not replace the framework's existing server-authoritative gameplay paths. After profile binding, the normal replicated systems continue to operate:

- Inventory and runtime item instances
- Equipment/held presentation
- Quick Access owner state
- combat and damage
- Stats/progression/skills including Mining
- building placement/demolition/ownership
- Doors, Windows and Lights
- Storage transfer
- Production/Furnace queues
- Settlement ownership/residents
- Woodcutting/Mining reward authority
- chat routing
- world time and other replicated world state

Clients submit gameplay intent through the existing RPC paths; the server validates and mutates authoritative state.

**Custom project gameplay is not made network-correct merely by inheriting a framework class.** Any new custom mutable gameplay state must still follow Unreal replication/RPC authority rules. v2.18 removes the account/save identity ambiguity; it does not claim to automatically network arbitrary project Blueprint variables that were never replicated.

---

## 9. Save schemas

v2.18.5 keeps character payload v5 but advances the world header to v10 for account-scope binding:

- Character save: **v5**
- World save: **v10** (account-scoped for logged-in standalone/listen-host sessions)

It adds a separate account-profile metadata save with its own version **1**.

Existing v2.17 local accounts migrate lazily on successful login: if their account profile metadata save does not exist, it is created automatically without changing their AccountId/CharacterId or character save slot.

---

## 10. Network limitations

Direct-IP Unreal networking does not automatically provide:

- router/NAT traversal
- UPnP port mapping
- firewall configuration
- server browser/discovery
- DDoS protection
- public Internet credential authentication
- cloud character portability
- encrypted account backend traffic

For Internet hosting, the host may need UDP port forwarding/firewall configuration for the chosen Unreal listen port. Exact transport requirements also depend on the project's Online Subsystem/network driver configuration.

For a public production service, use Steam/EOS/another trusted platform or your own backend for identity/session discovery and bind the resulting trusted account identifier into the RPG profile layer.

---

## 11. Recommended PIE / packaged QA

### Fresh account

1. Delete/create a test account.
2. Create Account from Login UI.
3. Confirm Main Menu opens.
4. Start Single Player.
5. Confirm Starting Items appear exactly once.
6. Change Inventory + Quick Access, wait past the debounce or call `SaveNow`.
7. Return/restart and verify exact state restores.

### Listen host

1. Login with Account A.
2. Host & Play on port 7777.
3. Verify host pawn spawns and Persistence reports the Account A character slot.
4. Build/modify a structure and save the world.

### Remote client

Use a second process/standalone client where possible.

1. Login with Account B.
2. Join the host IP.
3. Confirm the client waits for profile synchronization before pawn spawn.
4. Verify Account B gets a different CharacterId/slot from Account A.
5. Transfer items, alter Quick Access, mine/craft/build, then disconnect normally.
6. Rejoin the same host with Account B and verify the server restores Account B state.
7. Confirm Account B cannot modify Account A personal structures unless faction permissions explicitly allow it.

### Duplicate identity

Attempt a second simultaneous connection using Account B. The host should reject the duplicate profile rather than creating two pawns sharing one persistence identity.

### Failure path

Join an unavailable IP/port. Verify the network/travel failure is reported and the project returns to the configured Main Menu when that option is enabled.

> Multiple PIE windows can share the same machine SaveGame directory/account index. For final account/network identity testing, separate standalone processes or packaged builds provide a more realistic client/host boundary.

---

## v2.18.2 frontend -> gameplay handoff guarantees

The frontend intentionally uses `FInputModeUIOnly`. Before any gameplay travel, `ARPGFrontendPlayerController` now:

1. removes the active Login/Main Menu widget,
2. switches to `FInputModeGameOnly`,
3. hides the mouse cursor,
4. clears move-input suppression,
5. clears look-input suppression, and
6. only then starts Single Player / Listen Host / ClientTravel.

`ARPGPlayerController` repeats the GameOnly restoration on gameplay BeginPlay as a second guard against viewport/input state surviving local travel.

For a newly created account, the expected first Single Player entry is therefore:

`Account created (stable AccountId + CharacterId) -> travel -> ARPGGameMode -> identity accepted -> pawn spawned/possessed -> Persistence sees no character save -> Starting Items + Quick Access seed once -> first character snapshot`

If testing an account that was already used during a broken pre-v2.18.2 session, that account may legitimately contain an empty character snapshot. Do not delete or overwrite it automatically: use a newly created test account when validating the fresh-character branch, or deliberately remove the test SaveGame outside the framework if the data is disposable.
