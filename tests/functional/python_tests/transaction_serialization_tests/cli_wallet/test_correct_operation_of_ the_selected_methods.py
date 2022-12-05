import test_tools as tt


def test_transfer_operation_legacy_serialization(wallet_with_legacy_serialization):
    wallet_with_legacy_serialization.api.create_account('initminer', 'alice', '{}')
    wallet_with_legacy_serialization.api.transfer('initminer', 'alice', tt.Asset.Test(1000), 'memo')
    transaction = wallet_with_legacy_serialization.api.get_account('alice')

    assert transaction['balance'] == tt.Asset.Test(1000)


def test_transfer_operation_hf26_serialization(wallet_with_hf26_serialization):
    wallet_with_hf26_serialization.api.create_account('initminer', 'alice', '{}')
    wallet_with_hf26_serialization.api.transfer('initminer', 'alice', tt.Asset.Test(1000), 'memo')
    transaction = wallet_with_hf26_serialization.api.get_account('alice')

    assert transaction['balance'] == tt.Asset.Test(1000).as_nai()


def test_create_account_operation_legacy_serialization(wallet_with_legacy_serialization):
    wallet_with_legacy_serialization.api.create_account('initminer', 'alice', '{}')
    accounts = wallet_with_legacy_serialization.api.list_accounts(0, 100)

    assert 'alice' in accounts


def test_create_account_operation_hf26_serialization(wallet_with_hf26_serialization):
    wallet_with_hf26_serialization.api.create_account('initminer', 'alice', '{}')
    accounts = wallet_with_hf26_serialization.api.list_accounts(0, 100)

    assert 'alice' in accounts
