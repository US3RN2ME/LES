//------------------------------------------------------------------------------------------------
//! Lightweight Event System (LES) for Arma Reforger
//!
//! A small, dependency-free pub/sub event bus for the Enfusion engine.
//! MIT licensed, no telemetry, no required setup — load the mod and it works.
//!
//! LES_EventBus is a PURE local dispatcher. It has no knowledge of networking,
//! GameMode, or replication. Replication is layered on top: the GameMode patch
//! (LES_GameModePatch.c / LES_EventReplicator.c) subscribes to
//! GetReplicationHook() and forwards events to clients via RPC. The bus itself
//! never touches the network.
//!
//! Quick start:
//! \code
//!   // Subscribe (typically in your component's OnPostInit):
//!   LES_EventBus.GetInstance().GetInvoker(LES_EEventType.PLAYER_KILLED).Insert(OnKill);
//!
//!   void OnKill(LES_EventPayload payload)
//!   {
//!       Print("Player " + payload.m_iTargetId + " was killed by " +
//!       payload.m_iInstigatorId);
//!   }
//!
//!   // Unsubscribe (in OnDelete) to avoid dangling callbacks:
//!   LES_EventBus.GetInstance().GetInvoker(LES_EEventType.PLAYER_KILLED).Remove(OnKill);
//! \endcode
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! All built-in event types dispatched by LES.
enum LES_EEventType {
  PLAYER_KILLED,
  PLAYER_CONNECTED,
  PLAYER_DISCONNECTED,
  ZONE_CAPTURED,
  VEHICLE_DESTROYED,
}

//------------------------------------------------------------------------------------------------
//! Data container passed to every event subscriber.
//!
//! Replicated to clients (when m_bReplicateToClients is true):
//!   m_iInstigatorId, m_iTargetId, m_sContext, and all string tags.
//!
//! Server-only (never leaves the server):
//!   m_UserData — attach any script object for server-side logic. Use string
//!   tags for any data that must also reach clients.
class LES_EventPayload {
  //! Player/entity ID of whoever caused the event (-1 if not applicable).
  int m_iInstigatorId;

  //! Player/entity ID of whoever received the event (-1 if not applicable).
  int m_iTargetId;

  //! Short freeform context string (zone name, death cause, prefab name, etc.).
  string m_sContext;

  //! Whether this event should be replicated to clients. Default true.
  //! Set to false to keep an event entirely server-side.
  bool m_bReplicateToClients;

  //! Set internally by Broadcast() so the replicator knows which built-in event
  //! type to send. Not intended to be set by mod code.
  int m_iReplicationEventType;

  //! Arbitrary server-only object. Use Class.Cast() on the receiving end.
  //! Never replicated.
  Class m_UserData;

  //! Arbitrary string key/value metadata. Replicated via SerializeTags().
  private ref map<string, string> m_mTags;

  //------------------------------------------------------------------------------------------------
  void LES_EventPayload(int instigatorId = -1, int targetId = -1,
                        string context = "", bool replicateToClients = true) {
    m_iInstigatorId = instigatorId;
    m_iTargetId = targetId;
    m_sContext = context;
    m_bReplicateToClients = replicateToClients;
    m_mTags = new map<string, string>();
  }

  //------------------------------------------------------------------------------------------------
  //! Attach a string key/value pair. Replicated to clients.
  void SetTag(string key, string value) { m_mTags[key] = value; }

  //------------------------------------------------------------------------------------------------
  //! Read a tag value, or empty string if the key is absent.
  string GetTag(string key) {
    if (m_mTags.Contains(key))
      return m_mTags[key];
    return string.Empty;
  }

  //------------------------------------------------------------------------------------------------
  //! True if the given tag key is present.
  bool HasTag(string key) { return m_mTags.Contains(key); }

  //------------------------------------------------------------------------------------------------
  //! Number of tags currently attached.
  int GetTagCount() { return m_mTags.Count(); }

  //------------------------------------------------------------------------------------------------
  //! Serialise all tags into a pipe-delimited string for RPC transport.
  //! Format: "key1=value1|key2=value2". Returns empty string when there are no
  //! tags.
  string SerializeTags() {
    if (m_mTags.IsEmpty())
      return string.Empty;

    string result;
    bool first = true;

    foreach (string key, string value : m_mTags) {
      if (!first)
        result += "|";
      result += key + "=" + value;
      first = false;
    }

    return result;
  }

  //------------------------------------------------------------------------------------------------
  //! Rebuild tags from a string produced by SerializeTags().
  void DeserializeTags(string serialized) {
    if (serialized.IsEmpty())
      return;

    array<string> pairs = {};
    serialized.Split("|", pairs, false);

    foreach (string pair : pairs) {
      array<string> kv = {};
      pair.Split("=", kv, false);
      if (kv.Count() == 2)
        m_mTags[kv[0]] = kv[1];
    }
  }
}

//------------------------------------------------------------------------------------------------
//! Callback signature for all LES subscriptions: void
//! MyCallback(LES_EventPayload payload)
void LES_EventCallbackMethod(LES_EventPayload payload);
typedef func LES_EventCallbackMethod;
typedef ScriptInvokerBase<LES_EventCallbackMethod> LES_ScriptInvoker;

//------------------------------------------------------------------------------------------------
//! Central pub/sub event bus. Access via LES_EventBus.GetInstance().
class LES_EventBus {
  private static ref LES_EventBus s_Instance;

  //! Built-in event invokers: one per LES_EEventType.
  private ref map<int, ref LES_ScriptInvoker> m_mInvokers;

  //! Custom event invokers: dynamic id -> invoker.
  private ref map<int, ref LES_ScriptInvoker> m_mCustomInvokers;

  //! Custom event registry: "modId:eventName" -> dynamic id.
  private ref map<string, int> m_mCustomEvents;

  //! Counter for assigning unique custom event IDs.
  private int m_iNextCustomId;

  //! Fired on the server for every replicable event. The GameMode subscribes
  //! here to forward events to clients over RPC, keeping the bus
  //! network-agnostic.
  private ref LES_ScriptInvoker m_ReplicationHook;

  //------------------------------------------------------------------------------------------------
  void LES_EventBus() {
    m_mInvokers = new map<int, ref LES_ScriptInvoker>();
    m_mCustomInvokers = new map<int, ref LES_ScriptInvoker>();
    m_mCustomEvents = new map<string, int>();
    m_iNextCustomId = 10000;
    m_ReplicationHook = new LES_ScriptInvoker();
  }

  //------------------------------------------------------------------------------------------------
  //! Returns the global singleton, creating it on first call.
  static LES_EventBus GetInstance() {
    if (!s_Instance)
      s_Instance = new LES_EventBus();
    return s_Instance;
  }

  //------------------------------------------------------------------------------------------------
  //! True if the bus singleton has been created. Lets callers check without
  //! forcing creation (e.g. during teardown).
  static bool IsInitialised() { return s_Instance != null; }

  //------------------------------------------------------------------------------------------------
  //! Destroy the singleton. Called by the GameMode patch on world unload so a
  //! fresh world starts with a clean bus and no stale subscriptions.
  static void _Reset() { s_Instance = null; }

  // -----------------------------------------------------------------------------------------
  // Built-in events
  // -----------------------------------------------------------------------------------------

  //------------------------------------------------------------------------------------------------
  //! Get the ScriptInvoker for a built-in event type, creating it on first use.
  //! Subscribe with GetInvoker(type).Insert(callback) and unsubscribe with
  //! GetInvoker(type).Remove(callback). Always pair the two — remove your
  //! callback in OnDelete so it never fires on a destroyed object.
  LES_ScriptInvoker GetInvoker(LES_EEventType eventType) {
    int key = eventType;
    if (!m_mInvokers.Contains(key))
      m_mInvokers[key] = new LES_ScriptInvoker();
    return m_mInvokers[key];
  }

  //------------------------------------------------------------------------------------------------
  //! Dispatch a built-in event to all local subscribers. On the server, also
  //! fires the replication hook (if the event is replicable) so clients receive
  //! it. Normally called by the built-in listeners; mods may call it to emit
  //! synthetic events.
  void Broadcast(LES_EEventType eventType, notnull LES_EventPayload payload) {
    DispatchLocal(eventType, payload);

    if (Replication.IsServer() && payload.m_bReplicateToClients) {
      payload.m_iReplicationEventType = eventType;
      m_ReplicationHook.Invoke(payload);
    }
  }

  //------------------------------------------------------------------------------------------------
  //! Dispatch to local subscribers only, skipping the replication hook.
  //! Called on clients after receiving an RPC, so the event isn't bounced back.
  void BroadcastLocal(LES_EEventType eventType,
                      notnull LES_EventPayload payload) {
    DispatchLocal(eventType, payload);
  }

  //------------------------------------------------------------------------------------------------
  //! True if an invoker has been created for the given built-in event, i.e. at
  //! least one subscriber has requested it. Lets callers skip building an
  //! expensive payload when nobody is listening. (Note: an invoker persists
  //! after its last unsubscribe, so this can return true with zero live
  //! callbacks — treat it as a cheap hint.)
  bool HasSubscribers(LES_EEventType eventType) {
    int key = eventType;
    return m_mInvokers.Contains(key);
  }

  //------------------------------------------------------------------------------------------------
  //! The replication hook. The GameMode subscribes here to forward replicable
  //! events to clients. Mods generally don't need to touch this.
  LES_ScriptInvoker GetReplicationHook() { return m_ReplicationHook; }

  // -----------------------------------------------------------------------------------------
  // Custom events
  // -----------------------------------------------------------------------------------------

  //------------------------------------------------------------------------------------------------
  //! Register a custom event and receive its unique runtime ID. Calling this
  //! twice with the same modId/eventName returns the same ID (and warns once).
  //! \param modId     Your mod's unique identifier string
  //! \param eventName Short descriptive name, e.g. "AIRDROP_CALLED"
  //! \return Stable integer ID for use with the custom-event methods below
  int RegisterCustomEvent(string modId, string eventName) {
    string key = modId + ":" + eventName;

    if (m_mCustomEvents.Contains(key)) {
      Print("[LES] Custom event already registered: " + key, LogLevel.WARNING);
      return m_mCustomEvents[key];
    }

    int id = m_iNextCustomId;
    m_iNextCustomId++;
    m_mCustomEvents[key] = id;

    Print("[LES] Registered custom event '" + key + "' -> id=" + id);
    return id;
  }

  //------------------------------------------------------------------------------------------------
  //! Look up a previously registered custom event ID, or -1 if it doesn't
  //! exist. Lets a subscriber resolve an ID registered by another script
  //! without re-registering.
  int FindCustomEvent(string modId, string eventName) {
    string key = modId + ":" + eventName;
    if (m_mCustomEvents.Contains(key))
      return m_mCustomEvents[key];
    return -1;
  }

  //------------------------------------------------------------------------------------------------
  //! Get the ScriptInvoker for a custom event ID, creating it on first use.
  //! Subscribe with GetCustomInvoker(id).Insert(callback) and unsubscribe with
  //! GetCustomInvoker(id).Remove(callback).
  LES_ScriptInvoker GetCustomInvoker(int eventId) {
    if (!m_mCustomInvokers.Contains(eventId))
      m_mCustomInvokers[eventId] = new LES_ScriptInvoker();
    return m_mCustomInvokers[eventId];
  }

  //------------------------------------------------------------------------------------------------
  //! Dispatch a custom event to its local subscribers.
  //! Custom events are local/server-only — they may carry arbitrary m_UserData
  //! that can't be replicated, so mods handle their own networking if they need
  //! it.
  void BroadcastCustom(int eventId, notnull LES_EventPayload payload) {
    if (!m_mCustomInvokers.Contains(eventId))
      return;
    m_mCustomInvokers[eventId].Invoke(payload);
  }

  // -----------------------------------------------------------------------------------------
  // Internal
  // -----------------------------------------------------------------------------------------

  //------------------------------------------------------------------------------------------------
  private void DispatchLocal(LES_EEventType eventType,
                             notnull LES_EventPayload payload) {
    int key = eventType;
    if (!m_mInvokers.Contains(key))
      return;
    m_mInvokers[key].Invoke(payload);
  }
}