# LES — Lightweight Event System

A small, dependency-free **publish/subscribe event bus** for Arma Reforger's Enfusion engine. LES gives mods a clean,
decoupled way to react to game events — and to define and broadcast their own — without each mod having to patch
base-game classes or reinvent server→client replication.

Built for Arma Reforger **1.7.x**. MIT licensed. No telemetry, no required setup — load the mod and it works.

---

## Why

Enfusion exposes plenty of useful callbacks (`GetOnPlayerKilled`, `GetOnPlayerConnected`, …), but consuming them from a
mod means `modded class SCR_BaseGameMode` and careful `Insert`/`Remove` bookkeeping. When several mods do this
independently, you get duplicated patches, ordering surprises, and no shared vocabulary for "a player was killed."

LES centralises that. One bus, a typed set of events, and a tiny `Subscribe` / `Unsubscribe` API. Events that should
reach clients are replicated for you over a reliable RPC, so a subscriber on a client lights up exactly like one on the
server.

## Design at a glance

The core (`LES_EventBus`) is a **pure local dispatcher** — it knows nothing about networking or the GameMode.
Replication is layered on top via dependency inversion: the bus exposes a *replication hook* (a `ScriptInvoker`), and
the GameMode patch subscribes to that hook and performs the RPC. This keeps the core testable and lets you use LES
purely locally if you never need replication.

```
 game callback ──> LES_Listeners ──> LES_EventBus.Broadcast()
                                          │
                        ┌─────────────────┼───────────────────┐
                        ▼                                      ▼
                 local subscribers                    replication hook (server)
                                                              │
                                                              ▼
                                                    RPC ──> clients ──> BroadcastLocal()
                                                                              │
                                                                              ▼
                                                                      local subscribers
```

| File                               | Responsibility                                                      |
|------------------------------------|---------------------------------------------------------------------|
| `LES_EventBus.c`                   | Core pub/sub dispatcher, payload type, custom-event registry        |
| `LES_GameModePatch.c`              | Initialisation + teardown; wires listeners and the replication hook |
| `LES_Listeners.c`                  | Translates built-in game callbacks into LES events                  |
| `LES_EventReplicator.c`            | Server→client RPC transport                                         |
| `Examples/LES_KillFeedComponent.c` | Worked example consumer (safe to delete)                            |

## Built-in events

| Event                 | Instigator       | Target           | Context / tags                |
|-----------------------|------------------|------------------|-------------------------------|
| `PLAYER_KILLED`       | killer player ID | victim player ID | `"killed"`                    |
| `PLAYER_CONNECTED`    | player ID        | —                | `"connected"`                 |
| `PLAYER_DISCONNECTED` | player ID        | —                | `"disconnected"`, tag `cause` |
| `VEHICLE_DESTROYED`   | killer player ID | —                | vehicle prefab name           |
| `ZONE_CAPTURED`       | *(reserved)*     |                  | *(see note)*                  |

> `ZONE_CAPTURED` is defined for forward compatibility. The 1.7.x capture API differs from earlier branches, so it is
> left unwired rather than shipped half-working — register it as a custom event in the meantime if you need it now.

---

## Usage

### Subscribe to a built-in event

```cpp
class MyConsumer : ScriptComponent
{
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);
        LES_EventBus.GetInstance().Subscribe(LES_EEventType.PLAYER_KILLED, OnKill);
    }

    override void OnDelete(IEntity owner)
    {
        if (LES_EventBus.IsInitialised())
            LES_EventBus.GetInstance().Unsubscribe(LES_EEventType.PLAYER_KILLED, OnKill);
        super.OnDelete(owner);
    }

    void OnKill(LES_EventPayload payload)
    {
        Print("killer=" + payload.m_iInstigatorId + " victim=" + payload.m_iTargetId);
    }
}
```

**Always pair `Subscribe` with `Unsubscribe`.** The `IsInitialised()` guard avoids recreating the bus during world
teardown just to remove a callback.

### Reading tags and context

```cpp
void OnDisconnect(LES_EventPayload payload)
{
    if (payload.HasTag("cause"))
        Print("left because: " + payload.GetTag("cause"));
}
```

### Broadcasting a built-in event yourself

Mods can emit synthetic events — every subscriber (local and, if replicable, remote) is notified:

```cpp
LES_EventPayload p = new LES_EventPayload(killerId, victimId, "killed");
LES_EventBus.GetInstance().Broadcast(LES_EEventType.PLAYER_KILLED, p);
```

### Server-only data

`m_UserData` carries any script object to local subscribers but is **never** replicated. Use string tags for anything
that must also reach clients.

```cpp
LES_EventPayload p = new LES_EventPayload(playerId);
p.m_UserData = mySensitiveServerObject;   // local only
p.SetTag("score", "1200");                // replicated
```

---

## Custom events

Define your own events without touching the core enum. IDs are assigned at runtime starting from `10000`.

```cpp
// Register once (e.g. in init). Returns a stable ID for this session.
int evtAirdrop = LES_EventBus.GetInstance().RegisterCustomEvent("MyMod", "AIRDROP_CALLED");

// Subscribe
LES_EventBus.GetInstance().SubscribeCustom(evtAirdrop, OnAirdrop);

// Broadcast
LES_EventPayload p = new LES_EventPayload(callerId, -1, "north_airfield");
LES_EventBus.GetInstance().BroadcastCustom(evtAirdrop, p);
```

Another script can resolve the same ID without re-registering:

```cpp
int evt = LES_EventBus.GetInstance().FindCustomEvent("MyMod", "AIRDROP_CALLED"); // -1 if absent
```

> Custom events are dispatched **locally** (they may carry non-replicable `m_UserData`). If you need a custom event on
> clients, send your own RPC, or broadcast a built-in replicable event with tags.

---

## API reference

### `LES_EventBus`

| Member                                                  | Description                                                              |
|---------------------------------------------------------|--------------------------------------------------------------------------|
| `static GetInstance()`                                  | Global singleton (created on first use)                                  |
| `static IsInitialised()`                                | True if the singleton exists; check before unsubscribing during teardown |
| `Subscribe(type, cb)` / `Unsubscribe(type, cb)`         | Built-in event subscription                                              |
| `Broadcast(type, payload)`                              | Dispatch locally and (server, if replicable) to clients                  |
| `BroadcastLocal(type, payload)`                         | Dispatch to local subscribers only                                       |
| `HasSubscribers(type)`                                  | Skip building a payload when nobody is listening                         |
| `GetInvoker(type)`                                      | Underlying `ScriptInvoker` for advanced use                              |
| `GetReplicationHook()`                                  | Server replication hook (used by the GameMode patch)                     |
| `RegisterCustomEvent(modId, name)`                      | Register a custom event, returns its ID                                  |
| `FindCustomEvent(modId, name)`                          | Resolve an existing custom event ID, or `-1`                             |
| `SubscribeCustom(id, cb)` / `UnsubscribeCustom(id, cb)` | Custom event subscription                                                |
| `BroadcastCustom(id, payload)`                          | Dispatch a custom event locally                                          |

### `LES_EventPayload`

| Field / method                                 | Description                              |
|------------------------------------------------|------------------------------------------|
| `m_iInstigatorId`, `m_iTargetId`               | Player/entity IDs (`-1` if N/A)          |
| `m_sContext`                                   | Freeform context string                  |
| `m_bReplicateToClients`                        | Set `false` to keep an event server-side |
| `m_UserData`                                   | Server-only object, never replicated     |
| `SetTag` / `GetTag` / `HasTag` / `GetTagCount` | Replicated string metadata               |

---

## Installation

1. Subscribe to LES on the Arma Reforger Workshop (or clone this repo into your addons folder).
2. Add it to your server's mod list / enable it in the launcher.
3. That's it — no World Editor setup. The GameMode patch initialises the bus and registers everything on load.

To consume events, attach `Examples/LES_KillFeedComponent` to your GameMode prefab as a starting point, then write your
own consumers using the same pattern.

## License

MIT — see [LICENSE](LICENSE). Use it in commercial or free mods, modify it, redistribute it. Attribution appreciated but
not required.
