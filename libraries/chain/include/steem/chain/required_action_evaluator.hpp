#pragma once

#include <steem/protocol/steem_required_actions.hpp>

#include <steem/chain/evaluator.hpp>

namespace steem { namespace chain {

using namespace steem::protocol;

#ifdef IS_TEST_NET
STEEM_DEFINE_ACTION_EVALUATOR( example_required, required_automated_action )
#endif

#ifdef STEEM_ENABLE_SMT
STEEM_DEFINE_ACTION_EVALUATOR( smt_refund, required_automated_action )
STEEM_DEFINE_ACTION_EVALUATOR( smt_contributor_payout, required_automated_action )
#endif

} } //steem::chain
