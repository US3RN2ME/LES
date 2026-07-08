//------------------------------------------------------------------------------------------------
//! LES Example — Kill Feed (zero-setup variant)
//!
//! Same behaviour as LES_KillFeedComponent, but wired through the GameMode
//! patch so it needs NO World Editor work — load the mod and watch the log. Use
//! THIS variant to test quickly; use the component variant
//! (LES_KillFeedComponent.c) as the reference pattern for real consumers that
//! live on their own entities.
//!
//! NOTE ON METHOD CHOICE
//!   The core framework already overrides OnWorldPostProcess (in
//!   LES_GameModePatch.c). Enforce Script forbids two overrides of the same
//!   method on one class, so this example hooks OnGameStart instead — it runs
//!   once the match starts, by which point the bus is already initialised. Pick
//!   a method the core doesn't use for your own GameMode-level consumers, or
//!   (cleaner) live on your own entity like the component variant does.
//!
//! Keep only ONE kill-feed example enabled at a time, or you'll get duplicate
//! lines.
//------------------------------------------------------------------------------------------------
modded class SCR_BaseGameMode {
   //------------------------------------------------------------------------------------------------
   override void OnGameStart() {
      super.OnGameStart();

      // The bus is already initialised by the core patch's OnWorldPostProcess.
      // Attach feed consumers to the events. Runs on every machine.
      LES_EventBus bus = LES_EventBus.GetInstance();
      bus.GetInvoker(LES_EEventType.PLAYER_KILLED).Insert(LES_KillFeed_OnKilled);
      bus.GetInvoker(LES_EEventType.PLAYER_CONNECTED).Insert(LES_KillFeed_OnConnected);
   }

   //------------------------------------------------------------------------------------------------
   protected void LES_KillFeed_OnKilled(LES_EventPayload payload) {
      string victim = LES_KillFeed_Name(payload.m_iTargetId);
      string line;

      if (payload.m_iInstigatorId <= 0 || payload.m_iInstigatorId == payload.m_iTargetId)
         line = victim + " died";
      else
         line = LES_KillFeed_Name(payload.m_iInstigatorId) + " eliminated " + victim;

      Print("[LES KillFeed] " + line);
   }

   //------------------------------------------------------------------------------------------------
   protected void LES_KillFeed_OnConnected(LES_EventPayload payload) {
      Print("[LES KillFeed] " + LES_KillFeed_Name(payload.m_iInstigatorId) + " joined the server");
   }

   //------------------------------------------------------------------------------------------------
   protected string LES_KillFeed_Name(int playerId) {
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
}
