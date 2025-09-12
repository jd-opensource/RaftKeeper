#include <gtest/gtest.h>

#include <Poco/Logger.h>

#include <Service/KeeperCommon.h>

#ifdef COMPATIBLE_MODE_ZOOKEEPER
#else
TEST(FeatureFlagsTest, TestFeatureFlags)
{
    RK::KeeperFeatureFlags feature_flags;
    feature_flags.enableFeatureFlag(RK::KeeperFeatureFlag::MULTI_READ);
    ASSERT_TRUE(feature_flags.isEnabled(RK::KeeperFeatureFlag::MULTI_READ));
    ASSERT_FALSE(feature_flags.isEnabled(RK::KeeperFeatureFlag::FILTERED_LIST));

    feature_flags.enableFeatureFlag(RK::KeeperFeatureFlag::FILTERED_LIST);
    ASSERT_TRUE(feature_flags.isEnabled(RK::KeeperFeatureFlag::FILTERED_LIST));

    feature_flags.enableFeatureFlag(RK::KeeperFeatureFlag::CREATE_IF_NOT_EXISTS);
    ASSERT_TRUE(feature_flags.isEnabled(RK::KeeperFeatureFlag::CREATE_IF_NOT_EXISTS));

    auto new_feature_flags = RK::KeeperFeatureFlags(feature_flags.getFeatureFlags());

    ASSERT_TRUE(new_feature_flags.isEnabled(RK::KeeperFeatureFlag::FILTERED_LIST));
    ASSERT_TRUE(new_feature_flags.isEnabled(RK::KeeperFeatureFlag::MULTI_READ));
    ASSERT_FALSE(new_feature_flags.isEnabled(RK::KeeperFeatureFlag::CHECK_NOT_EXISTS));
    ASSERT_TRUE(new_feature_flags.isEnabled(RK::KeeperFeatureFlag::CREATE_IF_NOT_EXISTS));

    auto default_feature_flags = RK::KeeperFeatureFlags(RK::CURRENT_KEEPER_FEATURE_FLAGS);
    ASSERT_TRUE(default_feature_flags.isEnabled(RK::KeeperFeatureFlag::FILTERED_LIST));
    ASSERT_TRUE(default_feature_flags.isEnabled(RK::KeeperFeatureFlag::MULTI_READ));
    ASSERT_TRUE(default_feature_flags.isEnabled(RK::KeeperFeatureFlag::CHECK_NOT_EXISTS));
    ASSERT_TRUE(default_feature_flags.isEnabled(RK::KeeperFeatureFlag::CREATE_IF_NOT_EXISTS));
}
#endif
