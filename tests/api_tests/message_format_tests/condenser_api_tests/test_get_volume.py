from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_volume(node):
    node.api.condenser.get_volume()
