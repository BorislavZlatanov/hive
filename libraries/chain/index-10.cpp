
#include <hive/chain/hive_object_types.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/sps_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_schedule.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_10( database& db )
{
  HIVE_ADD_CORE_INDEX(db, pending_required_action_index);
  HIVE_ADD_CORE_INDEX(db, pending_optional_action_index);
#ifdef HIVE_ENABLE_SMT
  HIVE_ADD_CORE_INDEX(db, smt_token_index);
  HIVE_ADD_CORE_INDEX(db, account_regular_balance_index);
  HIVE_ADD_CORE_INDEX(db, account_rewards_balance_index);
  HIVE_ADD_CORE_INDEX(db, nai_pool_index);
  HIVE_ADD_CORE_INDEX(db, smt_token_emissions_index);
  HIVE_ADD_CORE_INDEX(db, smt_contribution_index);
  HIVE_ADD_CORE_INDEX(db, smt_ico_index);
#endif
  HIVE_ADD_CORE_INDEX(db, proposal_index);
}

} }
