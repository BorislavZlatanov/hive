import test_tools as tt

from ..local_tools import run_for, date_from_now
from ....local_tools import create_account_and_fund_it


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_trade_history(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                                   tbds=tt.Asset.Tbd(100))

        wallet.api.create_order('alice', 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600)  # Sell 100 HIVE for 100 HBD
        wallet.api.create_order('bob', 0, tt.Asset.Tbd(100), tt.Asset.Test(100), False, 3600)  # Buy 100 HIVE for 100 HBD
    history = prepared_node.api.condenser.get_trade_history(date_from_now(weeks=-480), date_from_now(weeks=0), 10)
    assert len(history) != 0
