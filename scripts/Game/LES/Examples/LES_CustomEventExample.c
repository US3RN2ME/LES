//------------------------------------------------------------------------------------------------
//! LES Example — Custom Events with Replication (v1.1)
//!
//! Demonstrates the full custom-event API:
//!   RegisterCustomEvent   — define an event (idempotent: safe to call on any machine)
//!   GetCustomInvoker      — .Insert(cb) / .Remove(cb) to (un)subscribe
//!   BroadcastCustom       — emit; replicates to ALL CLIENTS automatically (v1.1)
//!   BroadcastCustomLocal  — emit strictly on this machine, never replicated
//!   FindCustomEvent       — resolve an ID registered elsewhere (-1 if absent)
//!
//! THE v1.1 HEADLINE — replication by key:
//!   Numeric event IDs are assigned in per-machine registration order, so they can't
//!   cross the network. LES replicates custom events by their "modId:eventName" string
//!   key instead. Producer and subscribers just register the same key on whatever
//!   machine they run on — no ordering, no coordination. m_UserData still never
//!   replicates; anything clients need goes into string tags.
//!
//! WHAT THIS DEMO DOES:
//!   Every machine (server + each client) subscribes to AIRDROP_CALLED in OnGameStart.
//!   The SERVER fires one replicated event 5s after start, and one local-only event
//!   10s after start. Expected log output:
//!     SERVER log: AIRDROP_CALLED received [SERVER]      <- local dispatch
//!                 SUPPLY_TICK  received [SERVER]        <- local-only, server sees it
//!     CLIENT log: AIRDROP_CALLED received [CLIENT]      <- arrived via RPC by key
//!                 (no SUPPLY_TICK line — it was BroadcastCustomLocal)
//!   That difference IS the demo: one method call put the event on every client.
//!
//! IMPORTANT — only ONE GameMode-based example can be enabled at a time.
//!   This file and LES_KillFeedGameMode.c both override OnGameStart on SCR_BaseGameMode,
//!   and Enforce Script forbids two overrides of the same method on one class. The
//!   component variant (LES_KillFeedComponent.c) never conflicts.
//------------------------------------------------------------------------------------------------

//! Shared identifiers. Any string pair works; keep modId unique to your mod.
class LES_AirdropEventIds {
   static const string MOD_ID = "LES_Example";
   static const string EVENT_AIRDROP = "AIRDROP_CALLED";
   static const string EVENT_TICK = "SUPPLY_TICK";
}

//------------------------------------------------------------------------------------------------
modded class SCR_BaseGameMode {
   protected int m_iAirdropEventId = -1;
   protected int m_iTickEventId = -1;

   //------------------------------------------------------------------------------------------------
   override void OnGameStart() {
      super.OnGameStart();

      LES_EventBus bus = LES_EventBus.GetInstance();

      // --- Register + subscribe on EVERY machine (server and clients alike). ---
      // Registration is idempotent and key-based, so each machine independently
      // registering "LES_Example:AIRDROP_CALLED" ends up wired to the same event.
      m_iAirdropEventId = bus.RegisterCustomEvent(LES_AirdropEventIds.MOD_ID, LES_AirdropEventIds.EVENT_AIRDROP);
      bus.GetCustomInvoker(m_iAirdropEventId).Insert(LES_OnAirdropCalled);

      m_iTickEventId = bus.RegisterCustomEvent(LES_AirdropEventIds.MOD_ID, LES_AirdropEventIds.EVENT_TICK);
      bus.GetCustomInvoker(m_iTickEventId).Insert(LES_OnSupplyTick);

      // --- Producers run on the SERVER only. Clients receive via replication. ---
      if (Replication.IsServer()) {
         GetGame().GetCallqueue().CallLater(LES_FireReplicatedAirdrop, 5000, false);
         GetGame().GetCallqueue().CallLater(LES_FireLocalTick, 10000, false);
      }
   }

   //------------------------------------------------------------------------------------------------
   //! Producer #1 — REPLICATED event (the v1.1 feature).
   //! One BroadcastCustom call on the server; every client's subscriber fires too.
   protected void LES_FireReplicatedAirdrop() {
      // FindCustomEvent resolves an ID registered elsewhere — shown here instead of
      // using m_iAirdropEventId directly, to demonstrate cross-script lookup.
      int id = LES_EventBus.GetInstance().FindCustomEvent(LES_AirdropEventIds.MOD_ID, LES_AirdropEventIds.EVENT_AIRDROP);
      if (id == -1)
         return;

      LES_EventPayload payload = new LES_EventPayload(-1, -1, "north_airfield");
      payload.SetTag("cargo", "supplies"); // tags replicate — clients will read these
      payload.SetTag("amount", "500");

      LES_EventBus.GetInstance().BroadcastCustom(id, payload);
      Print("[LES Example] SERVER broadcast AIRDROP_CALLED (replicated to clients)");
   }

   //------------------------------------------------------------------------------------------------
   //! Producer #2 — LOCAL-ONLY event, for contrast.
   //! BroadcastCustomLocal never touches the network: clients will NOT log this one.
   //! (Equivalent alternative: BroadcastCustom with payload.m_bReplicateToClients = false.)
   protected void LES_FireLocalTick() {
      LES_EventPayload payload = new LES_EventPayload(-1, -1, "hourly_restock");
      LES_EventBus.GetInstance().BroadcastCustomLocal(m_iTickEventId, payload);
      Print("[LES Example] SERVER broadcast SUPPLY_TICK (local only — clients stay silent)");
   }

   //------------------------------------------------------------------------------------------------
   //! Subscriber — runs wherever the event arrives. The [SERVER]/[CLIENT] suffix in
   //! the log line is what proves replication: the same handler fires on both sides.
   protected void LES_OnAirdropCalled(LES_EventPayload payload) {
      Print("[LES Example] AIRDROP_CALLED received " + LES_MachineTag() + " | at=" + payload.m_sContext + " cargo=" + payload.GetTag("cargo") + " amount=" + payload.GetTag("amount"));
   }

   //------------------------------------------------------------------------------------------------
   protected void LES_OnSupplyTick(LES_EventPayload payload) {
      Print("[LES Example] SUPPLY_TICK received " + LES_MachineTag() + " | context=" + payload.m_sContext);
   }

   //------------------------------------------------------------------------------------------------
   protected string LES_MachineTag() {
      if (Replication.IsServer())
         return "[SERVER]";
      return "[CLIENT]";
   }
}
