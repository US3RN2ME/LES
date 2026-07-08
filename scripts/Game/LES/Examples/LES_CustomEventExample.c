//------------------------------------------------------------------------------------------------
//! LES Example — Custom Events
//!
//! End-to-end demo of the custom-event API:
//!   RegisterCustomEvent  — define a new event and get a runtime ID
//!   SubscribeCustom      — listen for it
//!   BroadcastCustom      — emit it
//!   FindCustomEvent      — resolve the ID from another script without
//!   re-registering
//!
//! The scenario: a "producer" fires an AIRDROP_CALLED event a few seconds after
//! the match starts, carrying the drop location as context plus a couple of
//! tags. A "consumer" subscribes and logs the drop.
//!
//! Custom events are LOCAL/server-side: they may carry arbitrary m_UserData
//! that can't be replicated, so LES does not auto-replicate them. If you need a
//! custom event on clients, send your own RPC, or fold the data into a
//! replicable built-in event.
//!
//! IMPORTANT — only ONE GameMode-based example can be enabled at a time.
//!   This file and LES_KillFeedGameMode.c both override OnGameStart on
//!   SCR_BaseGameMode, and Enforce Script forbids two overrides of the same
//!   method on one class. To run this demo, remove (or comment out)
//!   LES_KillFeedGameMode.c. The component variant (LES_KillFeedComponent.c)
//!   does NOT conflict — it lives on its own entity — so you can keep that one
//!   alongside this file.
//------------------------------------------------------------------------------------------------

//! Shared identifiers so producer and consumer agree on the same event without
//! depending on call order. Any string pair works; keep modId unique to your
//! mod.
class LES_AirdropEventIds {
  static const string MOD_ID = "LES_Example";
  static const string EVENT_NAME = "AIRDROP_CALLED";
}

//------------------------------------------------------------------------------------------------
modded class SCR_BaseGameMode {
  protected int m_iAirdropEventId = -1;

  //------------------------------------------------------------------------------------------------
  override void OnGameStart() {
    super.OnGameStart();

    LES_EventBus bus = LES_EventBus.GetInstance();

    // --- Register the custom event (returns a stable ID for this session) ---
    m_iAirdropEventId = bus.RegisterCustomEvent(LES_AirdropEventIds.MOD_ID,
                                                LES_AirdropEventIds.EVENT_NAME);

    // --- Subscribe a consumer to it ---
    bus.GetCustomInvoker(m_iAirdropEventId).Insert(LES_OnAirdropCalled);

    // --- Demo producer: fire the event 5 seconds after start (server only) ---
    if (Replication.IsServer())
      GetGame().GetCallqueue().CallLater(LES_FireDemoAirdrop, 5000, false);
  }

  //------------------------------------------------------------------------------------------------
  //! Producer: build a payload and broadcast the custom event.
  protected void LES_FireDemoAirdrop() {
    // Another script could resolve the ID this way instead of holding a member
    // — both reach the same event:
    int id = LES_EventBus.GetInstance().FindCustomEvent(
        LES_AirdropEventIds.MOD_ID, LES_AirdropEventIds.EVENT_NAME);
    if (id == -1)
      return;

    LES_EventPayload payload = new LES_EventPayload(-1, -1, "north_airfield");
    payload.SetTag("cargo", "supplies");
    payload.SetTag("amount", "500");

    LES_EventBus.GetInstance().BroadcastCustom(id, payload);
    Print("[LES Example] Broadcast AIRDROP_CALLED");
  }

  //------------------------------------------------------------------------------------------------
  //! Consumer: react to the custom event.
  protected void LES_OnAirdropCalled(LES_EventPayload payload) {
    Print("[LES Example] Airdrop called at: " + payload.m_sContext +
          " | cargo=" + payload.GetTag("cargo") + " amount=" +
          payload.GetTag("amount") + " (" + payload.GetTagCount() + " tags)");
  }
}
