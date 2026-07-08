//------------------------------------------------------------------------------------------------
//! LES Example — Kill Feed
//!
//! A complete, self-contained example of consuming LES events. It subscribes to
//! PLAYER_KILLED and PLAYER_CONNECTED and prints a formatted feed line for
//! each. This doubles as a live demo (watch the log / extend it to on-screen
//! text) and as a reference for the recommended subscribe/unsubscribe
//! lifecycle.
//!
//! HOW TO USE THIS EXAMPLE
//!   Attach the LES_KillFeedComponent to any entity that exists on clients —
//!   the GameMode prefab is the simplest choice. The component subscribes in
//!   OnPostInit and unsubscribes in OnDelete, which is the pattern you should
//!   copy for your own consumers so callbacks never fire on a destroyed object.
//!
//! WHY A COMPONENT
//!   Components give you a natural OnPostInit / OnDelete pair that maps cleanly
//!   onto Subscribe / Unsubscribe. You can subscribe from anywhere (a manager,
//!   a menu, a static init) — this is just the most common and safest shape.
//!
//! This file is illustrative. Delete the Examples/ folder if you only want the
//! core framework; nothing else depends on it.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
class LES_KillFeedComponentClass : ScriptComponentClass {}

//------------------------------------------------------------------------------------------------
//! Listens for LES events and emits a human-readable kill feed.
class LES_KillFeedComponent : ScriptComponent {
   //------------------------------------------------------------------------------------------------
   override void OnPostInit(IEntity owner) {
      super.OnPostInit(owner);

      // Subscribe to the events we care about. Runs on every machine: the server
      // broadcasts locally and replicates, clients receive via RPC and broadcast
      // locally — so a client-side feed lights up from replicated events too.
      LES_EventBus bus = LES_EventBus.GetInstance();
      bus.GetInvoker(LES_EEventType.PLAYER_KILLED).Insert(OnPlayerKilled);
      bus.GetInvoker(LES_EEventType.PLAYER_CONNECTED).Insert(OnPlayerConnected);
   }

   //------------------------------------------------------------------------------------------------
   override void OnDelete(IEntity owner) {
      // Always mirror every Insert with a Remove here. If the bus has already
      // been torn down (world unload), IsInitialised() guards against recreating
      // it just to remove callbacks.
      if (LES_EventBus.IsInitialised()) {
         LES_EventBus bus = LES_EventBus.GetInstance();
         bus.GetInvoker(LES_EEventType.PLAYER_KILLED).Remove(OnPlayerKilled);
         bus.GetInvoker(LES_EEventType.PLAYER_CONNECTED).Remove(OnPlayerConnected);
      }

      super.OnDelete(owner);
   }

   //------------------------------------------------------------------------------------------------
   //! PLAYER_KILLED handler. Resolves player names and prints a feed line.
   protected void OnPlayerKilled(LES_EventPayload payload) {
      string killer = ResolveName(payload.m_iInstigatorId);
      string victim = ResolveName(payload.m_iTargetId);

      string line;
      if (payload.m_iInstigatorId == payload.m_iTargetId || payload.m_iInstigatorId <= 0)
         line = victim + " died";
      else
         line = killer + " eliminated " + victim;

      EmitFeedLine(line);
   }

   //------------------------------------------------------------------------------------------------
   //! PLAYER_CONNECTED handler.
   protected void OnPlayerConnected(LES_EventPayload payload) {
      EmitFeedLine(ResolveName(payload.m_iInstigatorId) + " joined the server");
   }

   //------------------------------------------------------------------------------------------------
   //! Turn a player ID into a display name, with a sensible fallback.
   protected string ResolveName(int playerId) {
      if (playerId <= 0)
         return "Unknown";

      PlayerManager pm = GetGame().GetPlayerManager();
      if (!pm)
         return "Player " + playerId;

      string name = pm.GetPlayerName(playerId);
      if (name.IsEmpty())
         return "Player " + playerId;

      return name;
   }

   //------------------------------------------------------------------------------------------------
   //! Output a single feed line. Swap the Print() for an on-screen hint, chat
   //! message, or custom HUD widget to surface the feed in-game.
   protected void EmitFeedLine(string line) {
      Print("[LES KillFeed] " + line);
   }
}
