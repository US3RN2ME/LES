//------------------------------------------------------------------------------------------------
//! LES — server-to-client RPC bridge.
//!
//! This file extends SCR_BaseGameMode with the RPC machinery needed to forward
//! replicable LES events to clients. It works OUT OF THE BOX: because the
//! GameMode entity always exists and always has an RplComponent, no manual
//! component setup is required. The mod is loaded → replication is live.
//!
//! Why on the GameMode and not a separate component:
//!   RPCs can only be sent from an entity that has an RplComponent. The
//!   GameMode already satisfies this, so piggybacking on it removes the setup
//!   burden that a standalone component would impose (manually attaching it in
//!   the World Editor).
//!
//! The actual subscription to the EventBus replication hook lives in
//! LES_GameModePatch.c (the OnWorldPostProcess override), to keep a single
//! initialisation entry point. These RPC methods are the transport layer only.
//------------------------------------------------------------------------------------------------
modded class SCR_BaseGameMode {
   //------------------------------------------------------------------------------------------------
   //! Server-side hook target. Subscribed to LES_EventBus.GetReplicationHook()
   //! in LES_GameModePatch. Fires for every replicable event and sends it via
   //! RPC.
   //! \param payload The event payload being broadcast on the server
   void LES_OnReplicableEvent(LES_EventPayload payload) {
      if (!Replication.IsServer())
         return;

      Rpc(LES_RpcReceiveEvent, payload.m_iReplicationEventType, payload.m_iInstigatorId, payload.m_iTargetId,
          payload.m_sContext, payload.SerializeTags());
   }

   //------------------------------------------------------------------------------------------------
   //! Executed on every client. Rebuilds the payload from primitives and
   //! dispatches it to local subscribers via the bus (without re-replicating).
   [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)] protected void LES_RpcReceiveEvent(
       int eventType, int instigatorId, int targetId, string context, string serializedTags) {
      LES_EventPayload payload = new LES_EventPayload(instigatorId, targetId, context, false);
      payload.DeserializeTags(serializedTags);
      LES_EventBus.GetInstance().BroadcastLocal(eventType, payload);
   }
}
