#pragma once

#include <cstdint>
#include <string>


namespace Coordination
{

using XID = int32_t;

static constexpr XID WATCH_XID = -1;
static constexpr XID PING_XID = -2;
static constexpr XID AUTH_XID = -4;
static constexpr XID NEW_SESSION_XID = -256;
static constexpr XID UPDATE_SESSION_XID = -512;
/// Used when RaftKeeper server close a session.
/// But when client send a close request xid can be any digit.
static constexpr XID CLOSE_XID = 0x7FFFFFFF;

enum class OpNum : int32_t
{
    Unspecified = -199, /// Used to request which has no opnum, such as triggered watch response
    Close = -11,
    Error = -1,
    Create = 1,
    Remove = 2,
    Exists = 3,
    Get = 4,
    Set = 5,
    GetACL = 6,
    SetACL = 7,
    SimpleList = 8,
    Sync = 9,
    Heartbeat = 11,
    List = 12,
    Check = 13,
    Multi = 14,
    MultiRead = 22,
    Auth = 100,
    SetWatches = 101,
    NewSession = -10, /// Used to create new session.
    OldNewSession = 997, /// Same with NewSession, just for backward compatibility

    FilteredList = 500, /// Special operation only used in ClickHouse.
    UpdateSession = 998, /// Special internal request. Used to session reconnect.
};

std::string toString(OpNum op_num);
OpNum getOpNum(int32_t raw_op_num);

static constexpr int32_t ZOOKEEPER_PROTOCOL_VERSION = 0;
static constexpr int32_t CLIENT_HANDSHAKE_LENGTH = 44;
static constexpr int32_t CLIENT_HANDSHAKE_LENGTH_WITH_READONLY = 45;
static constexpr int32_t SERVER_HANDSHAKE_LENGTH = 36;
static constexpr int32_t PASSWORD_LENGTH = 16;

/// ZooKeeper has 1 MB node size and serialization limit by default,
/// but it can be raised up, so we have a slightly larger limit on our side.
static constexpr int32_t MAX_STRING_OR_ARRAY_SIZE = 1 << 28; /// 256 MiB
static constexpr int32_t DEFAULT_SESSION_TIMEOUT_MS = 30000;
static constexpr int32_t DEFAULT_MIN_SESSION_TIMEOUT_MS = 1000;
static constexpr int32_t DEFAULT_MAX_SESSION_TIMEOUT_MS = 3600000;
static constexpr int32_t DEFAULT_OPERATION_TIMEOUT_MS = 3000;
static constexpr int32_t ELECTION_TIMEOUT_LOWER_BOUND_MS = 3000;
static constexpr int32_t ELECTION_TIMEOUT_UPPER_BOUND_MS = 5000;

}
