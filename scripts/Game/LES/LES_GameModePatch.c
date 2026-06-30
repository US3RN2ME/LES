//------------------------------------------------------------------------------------------------
//! LES — initialisation entry point.
//!
//! Wires LES into the game on world load and tears it down on unload.
//! Initialises the EventBus on every machine; on the server, connects the
//! replication hook and registers the built-in listeners (defined in
//! LES_Listeners.c). Works out of the box once the mod loads — no World Editor
//! setup required.
//!
//! Responsibility split across the SCR_BaseGameMode partial:
//!   LES_GameModePatch.c    — this file: initialisation + teardown
//!   LES_Listeners.c        — game event callbacks that feed the bus
//!   LES_EventReplicator.c  — RPC transport (server <-> client)
//------------------------------------------------------------------------------------------------
modded class SCR_BaseGameMode {
   //------------------------------------------------------------------------------------------------
   override void OnWorldPostProcess(World world) {
      super.OnWorldPostProcess(world);

      // Bus exists on every machine so clients can receive replicated events.
      LES_EventBus bus = LES_EventBus.GetInstance();

      if (!Replication.IsServer()) {
         Print("[LES] Initialised (client) | EventBus ready");
         return;
      }

      // Server: forward replicable events to clients (transport in
      // LES_EventReplicator.c).
      bus.GetReplicationHook().Insert(LES_OnReplicableEvent);

      // Server: register built-in game listeners (callbacks in LES_Listeners.c).
      GetOnPlayerKilled().Insert(LES_OnPlayerKilled);
      GetOnPlayerConnected().Insert(LES_OnPlayerConnected);
      GetOnPlayerDisconnected().Insert(LES_OnPlayerDisconnected);
      GetOnControllableDestroyed().Insert(LES_OnControllableDestroyed);

      Print("[LES] Initialised (server) | replication active, listeners "
            "registered");
   }

   //------------------------------------------------------------------------------------------------
   //! Destroy the bus singleton when the world unloads so the next world starts
   //! clean, with no stale subscriptions carried over from a previous session.
   override void OnGameEnd() {
      LES_EventBus._Reset();
      super.OnGameEnd();
   }
}
