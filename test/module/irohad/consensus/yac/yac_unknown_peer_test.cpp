/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "consensus/yac/storage/yac_proposal_storage.hpp"

#include "framework/test_subscriber.hpp"
#include "module/irohad/consensus/yac/yac_fixture.hpp"

using ::testing::_;
using ::testing::An;
using ::testing::AtLeast;
using ::testing::Return;

using namespace iroha::consensus::yac;
using namespace framework::test_subscriber;
using namespace std;

/**
 * @given initialized yac
 * @when receive vote from unknown peer
 * @then commit not emitted
 */
TEST_F(YacTest, UnknownVoteBeforeCommit) {
  auto my_order = ClusterOrdering::create(default_peers);
  ASSERT_TRUE(my_order);
  initYac(my_order.value());

  // verify that commit not emitted
  auto wrapper = make_test_subscriber<CallExact>(yac->onOutcome(), 0);
  wrapper.subscribe();

  EXPECT_CALL(*network, sendState(_, _)).Times(0);

  EXPECT_CALL(*crypto, verify(_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(Return(true));

  YacHash my_hash{iroha::consensus::Round{1, 1}, "my_proposal", "my_block"};

  // send enough votes for next valid to make a commit
  for (auto i = 0; i < 4; ++i) {
    yac->onState({createVote(my_hash, std::to_string(i))});
  }

  // send a vote from unknown peer
  yac->onState({createVote(my_hash, "unknown")});

  ASSERT_TRUE(wrapper.validate());
}

/**
 * @given initialized yac
 * AND received commit
 * @when receive vote from unknown peer for committed hash
 * @then commit not emitted
 */
TEST_F(YacTest, UnknownVoteAfterCommit) {
  auto my_peers = decltype(default_peers)(
      {default_peers.begin(), default_peers.begin() + 4});
  ASSERT_EQ(4, my_peers.size());

  auto my_order = ClusterOrdering::create(my_peers);
  ASSERT_TRUE(my_order);

  initYac(my_order.value());

  EXPECT_CALL(*network, sendState(_, _)).Times(0);

  EXPECT_CALL(*timer, deny()).Times(AtLeast(1));

  EXPECT_CALL(*crypto, verify(_)).Times(1).WillRepeatedly(Return(true));

  YacHash my_hash(iroha::consensus::Round{1, 1}, "proposal_hash", "block_hash");

  std::vector<VoteMessage> votes;

  for (auto i = 0; i < 3; ++i) {
    votes.push_back(createVote(my_hash, std::to_string(i)));
  };
  yac->onState(votes);

  VoteMessage vote;
  vote.hash = my_hash;
  std::string unknown = "unknown";
  vote.signature = createSig(unknown);
  yac->onState({vote});
}
