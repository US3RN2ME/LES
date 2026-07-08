//------------------------------------------------------------------------------------------------
//! LES — GameMode integration (initialisation, listeners, and replication
//! transport).
//!
//! Everything that patches SCR_BaseGameMode lives in THIS single file on
//! purpose. Enforce Script requires that a method referenced by Insert() is
//! visible in the same compilation scope as the Insert() call. Splitting the
//! registration calls (OnWorldPostProcess) from the listener/RPC method bodies
//! across separate modded-class files triggers "Can't make callback from
//! unknown method". Keeping them together avoids that entirely.
//!
//! Responsibilities, all on the SCR_BaseGameMode partial:
//!   OnWorldPostProcess — init the bus; on the server wire the replication hook
//!   and register
//!                        the built-in game listeners.
//!   OnGameEnd          — tear the bus down so the next world starts clean.
//!   LES_On*            — listener bodies that translate base-game callbacks
//!   into LES events. LES_OnReplicableEvent / LES_RpcReceiveEvent —
//!   server→client RPC transport.
//!
//! Works out of the box: the GameMode entity always exists and always has an
//! RplComponent, so no World Editor setup is required for replication.
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

    // Server: forward replicable events to clients (see LES_OnReplicableEvent
    // below).
    bus.GetReplicationHook().Insert(LES_OnReplicableEvent);

    // Server: register built-in game listeners (bodies below, same file/scope).
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

  // ===========================================================================================
  // Built-in game listeners (server-only; registration above is guarded by
  // IsServer)
  // ===========================================================================================

  //------------------------------------------------------------------------------------------------
  //! PLAYER_KILLED — m_iInstigatorId = killer, m_iTargetId = victim
  protected void LES_OnPlayerKilled(notnull SCR_InstigatorContextData data) {
    LES_EventPayload payload = new LES_EventPayload(
        data.GetKillerPlayerID(), data.GetVictimPlayerID(), "killed");
    LES_EventBus.GetInstance().Broadcast(LES_EEventType.PLAYER_KILLED, payload);
    Print("[LES] PLAYER_KILLED | victim=" + data.GetVictimPlayerID() +
          " killer=" + data.GetKillerPlayerID());
  }

  //------------------------------------------------------------------------------------------------
  //! PLAYER_CONNECTED — m_iInstigatorId = player ID
  protected void LES_OnPlayerConnected(int playerId) {
    LES_EventPayload payload = new LES_EventPayload(playerId, -1, "connected");
    LES_EventBus.GetInstance().Broadcast(LES_EEventType.PLAYER_CONNECTED,
                                         payload);
    Print("[LES] PLAYER_CONNECTED | id=" + playerId);
  }

  //------------------------------------------------------------------------------------------------
  //! PLAYER_DISCONNECTED — m_iInstigatorId = player ID, tag "cause" =
  //! KickCauseCode
  protected void LES_OnPlayerDisconnected(int playerId, KickCauseCode cause,
                                          int timeout) {
    LES_EventPayload payload =
        new LES_EventPayload(playerId, -1, "disconnected");
    payload.SetTag("cause", cause.ToString());
    LES_EventBus.GetInstance().Broadcast(LES_EEventType.PLAYER_DISCONNECTED,
                                         payload);
    Print("[LES] PLAYER_DISCONNECTED | id=" + playerId + " cause=" + cause);
  }

  //------------------------------------------------------------------------------------------------
  //! VEHICLE_DESTROYED — m_iInstigatorId = killer, m_sContext = prefab name.
  //! Infantry deaths are filtered out; only Vehicle entities pass through.
  protected void LES_OnControllableDestroyed(
      notnull SCR_InstigatorContextData data) {
    IEntity entity = data.GetVictimEntity();
    if (!entity || !Vehicle.Cast(entity))
      return;

    string prefabName = string.Empty;
    EntityPrefabData prefabData = entity.GetPrefabData();
    if (prefabData)
      prefabName = prefabData.GetPrefabName();

    LES_EventPayload payload =
        new LES_EventPayload(data.GetKillerPlayerID(), -1, prefabName);
    LES_EventBus.GetInstance().Broadcast(LES_EEventType.VEHICLE_DESTROYED,
                                         payload);
    Print("[LES] VEHICLE_DESTROYED | prefab=" + prefabName);
  }

  // ===========================================================================================
  // Server→client RPC transport
  // ===========================================================================================

  //------------------------------------------------------------------------------------------------
  //! Server-side hook target. Subscribed to LES_EventBus.GetReplicationHook()
  //! above. Fires for every replicable event and sends it via RPC.
  void LES_OnReplicableEvent(LES_EventPayload payload) {
    if (!Replication.IsServer())
      return;

    Rpc(LES_RpcReceiveEvent, payload.m_iReplicationEventType,
        payload.m_iInstigatorId, payload.m_iTargetId, payload.m_sContext,
        payload.SerializeTags());
  }

  //------------------------------------------------------------------------------------------------
  //! Executed on every client. Rebuilds the payload from primitives and
  //! dispatches it to local subscribers via the bus (without re-replicating).
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)] protected void
  LES_RpcReceiveEvent(int eventType, int instigatorId, int targetId,
                      string context, string serializedTags) {
    LES_EventPayload payload =
        new LES_EventPayload(instigatorId, targetId, context, false);
    payload.DeserializeTags(serializedTags);
    LES_EventBus.GetInstance().BroadcastLocal(eventType, payload);
  }
}