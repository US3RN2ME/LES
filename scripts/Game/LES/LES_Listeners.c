//------------------------------------------------------------------------------------------------
//! LES — built-in game event listeners.
//!
//! These methods are registered on the server in LES_GameModePatch.c. Each one
//! translates a base-game callback into an LES event and broadcasts it on the
//! bus. Part of the SCR_BaseGameMode partial — see LES_GameModePatch.c for the
//! wiring.
//!
//! All listeners run server-only; the registration in OnWorldPostProcess is
//! already guarded by Replication.IsServer(), so no per-method guard is needed
//! here.
//------------------------------------------------------------------------------------------------
modded class SCR_BaseGameMode {
   //------------------------------------------------------------------------------------------------
   //! PLAYER_KILLED — m_iInstigatorId = killer, m_iTargetId = victim
   protected void LES_OnPlayerKilled(notnull SCR_InstigatorContextData data) {
      LES_EventPayload payload = new LES_EventPayload(data.GetKillerPlayerID(), data.GetVictimPlayerID(), "killed");
      LES_EventBus.GetInstance().Broadcast(LES_EEventType.PLAYER_KILLED, payload);
      Print("[LES] PLAYER_KILLED | victim=" + data.GetVictimPlayerID() + " killer=" + data.GetKillerPlayerID());
   }

   //------------------------------------------------------------------------------------------------
   //! PLAYER_CONNECTED — m_iInstigatorId = player ID
   protected void LES_OnPlayerConnected(int playerId) {
      LES_EventPayload payload = new LES_EventPayload(playerId, -1, "connected");
      LES_EventBus.GetInstance().Broadcast(LES_EEventType.PLAYER_CONNECTED, payload);
      Print("[LES] PLAYER_CONNECTED | id=" + playerId);
   }

   //------------------------------------------------------------------------------------------------
   //! PLAYER_DISCONNECTED — m_iInstigatorId = player ID, tag "cause" =
   //! KickCauseCode
   protected void LES_OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout) {
      LES_EventPayload payload = new LES_EventPayload(playerId, -1, "disconnected");
      payload.SetTag("cause", cause.ToString());
      LES_EventBus.GetInstance().Broadcast(LES_EEventType.PLAYER_DISCONNECTED, payload);
      Print("[LES] PLAYER_DISCONNECTED | id=" + playerId + " cause=" + cause);
   }

   //------------------------------------------------------------------------------------------------
   //! VEHICLE_DESTROYED — m_iInstigatorId = killer, m_sContext = prefab name.
   //! Infantry deaths are filtered out; only Vehicle entities pass through.
   protected void LES_OnControllableDestroyed(notnull SCR_InstigatorContextData data) {
      IEntity entity = data.GetVictimEntity();
      if (!entity || !Vehicle.Cast(entity))
         return;

      string prefabName = string.Empty;
      EntityPrefabData prefabData = entity.GetPrefabData();
      if (prefabData)
         prefabName = prefabData.GetPrefabName();

      LES_EventPayload payload = new LES_EventPayload(data.GetKillerPlayerID(), -1, prefabName);
      LES_EventBus.GetInstance().Broadcast(LES_EEventType.VEHICLE_DESTROYED, payload);
      Print("[LES] VEHICLE_DESTROYED | prefab=" + prefabName);
   }
}
