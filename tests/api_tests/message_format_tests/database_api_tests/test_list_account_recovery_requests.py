from ..local_tools import request_account_recovery


def test_list_account_recovery_requests(node, wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    request_account_recovery(wallet, 'alice')
    requests = node.api.database.list_account_recovery_requests(start='', limit=100, order='by_account')['requests']
    assert len(requests) != 0
