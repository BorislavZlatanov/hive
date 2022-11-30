from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_witness_by_account(node):
    node.api.condenser.get_witness_by_account('initminer')
