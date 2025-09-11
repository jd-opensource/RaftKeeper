#include <Service/KeeperCommon.h>

#include <Service/formatHex.h>
#include <common/logger_useful.h>

namespace RK
{

String ErrorRequest::toString() const
{
    return fmt::format(
        "#{}#{}#{} accepted:{} error_code:{}",
        toHexString(session_id),
        xid,
        Coordination::toString(opnum),
        accepted,
        RK::toString(error_code));
}

RequestId ErrorRequest::getRequestId() const
{
    return {session_id, xid};
}

String RequestId::toString() const
{
    return fmt::format("#{}#{}", toHexString(session_id), xid);
}

bool RequestId::operator==(const RequestId & other) const
{
    return session_id == other.session_id && xid == other.xid;
}

std::size_t RequestId::RequestIdHash::operator()(const RequestId & request_id) const
{
    std::size_t seed = 0;
    std::hash<int64_t> hash64;
    std::hash<int32_t> hash32;

    seed ^= hash64(request_id.session_id);
    seed ^= hash32(request_id.xid);

    return seed;
}

String RequestForSession::toString() const
{
    return (server_id != -1 && client_id != -1)
        ? fmt::format(
              "[session:{} request:{} create_time:{} server_id:{} client_id:{}]",
              toHexString(session_id),
              request->toString(),
              create_time,
              server_id,
              client_id)
        : fmt::format("[session:{} request:{} create_time:{}]", toHexString(session_id), request->toString(), create_time);
}

String RequestForSession::toSimpleString() const
{
    return fmt::format("#{}#{}#{}", toHexString(session_id), request->xid, Coordination::toString(request->getOpNum()));
}

RequestId RequestForSession::getRequestId() const
{
    return {session_id, request->xid};
}

/// Is new session request or update session request
bool isSessionRequest(Coordination::OpNum opnum)
{
    return opnum == Coordination::OpNum::NewSession || opnum == Coordination::OpNum::OldNewSession
        || opnum == Coordination::OpNum::UpdateSession;
}

bool isSessionRequest(const Coordination::ZooKeeperRequestPtr & request)
{
    return isSessionRequest(request->getOpNum());
}

bool isNewSessionRequest(Coordination::OpNum opnum)
{
    return opnum == Coordination::OpNum::NewSession || opnum == Coordination::OpNum::OldNewSession;
}


#ifdef COMPATIBLE_MODE_ZOOKEEPER
#else
std::pair<size_t, size_t> getByteAndBitIndex(size_t num)
{
    size_t byte_idx = num / 8;
    auto bit_idx = (7 - num % 8);
    return {byte_idx, bit_idx};
}


KeeperFeatureFlags::KeeperFeatureFlags()
{
    /// get byte idx of largest value
    auto [byte_idx, _] = getByteAndBitIndex(static_cast<size_t>(KeeperFeatureFlag::MAX) - 1);
    feature_flags = std::string(byte_idx + 1, 0);
}

KeeperFeatureFlags::KeeperFeatureFlags(std::string feature_flags_)
    : feature_flags(std::move(feature_flags_))
{}

void KeeperFeatureFlags::fromApiVersion(KeeperApiVersion keeper_api_version)
{
    if (keeper_api_version == KeeperApiVersion::ZOOKEEPER_COMPATIBLE)
        return;

    if (keeper_api_version >= KeeperApiVersion::WITH_FILTERED_LIST)
        enableFeatureFlag(KeeperFeatureFlag::FILTERED_LIST);

    if (keeper_api_version >= KeeperApiVersion::WITH_MULTI_READ)
        enableFeatureFlag(KeeperFeatureFlag::MULTI_READ);

    if (keeper_api_version >= KeeperApiVersion::WITH_CHECK_NOT_EXISTS)
        enableFeatureFlag(KeeperFeatureFlag::CHECK_NOT_EXISTS);
}

bool KeeperFeatureFlags::isEnabled(KeeperFeatureFlag feature_flag) const
{
    auto [byte_idx, bit_idx] = getByteAndBitIndex(static_cast<size_t>(feature_flag));

    if (byte_idx > feature_flags.size())
        return false;

    return feature_flags[byte_idx] & (1 << bit_idx);
}

void KeeperFeatureFlags::setFeatureFlags(std::string feature_flags_)
{
    feature_flags = std::move(feature_flags_);
}

void KeeperFeatureFlags::enableFeatureFlag(KeeperFeatureFlag feature_flag)
{
    auto [byte_idx, bit_idx] = getByteAndBitIndex(static_cast<size_t>(feature_flag));
    assert(byte_idx < feature_flags.size());

    feature_flags[byte_idx] |= (1 << bit_idx);
}

void KeeperFeatureFlags::disableFeatureFlag(KeeperFeatureFlag feature_flag)
{
    auto [byte_idx, bit_idx] = getByteAndBitIndex(static_cast<size_t>(feature_flag));
    assert(byte_idx < feature_flags.size());

    feature_flags[byte_idx] &= ~(1 << bit_idx);
}

const std::string & KeeperFeatureFlags::getFeatureFlags() const
{
    return feature_flags;
}
#endif

}
