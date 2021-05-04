import pytest

from test_library import World


def test_attaching_wallet_to_node():
    with World() as world:
        node = world.create_init_node()
        node.run()

        wallet = node.attach_wallet()
        assert wallet.is_running()


def test_attaching_wallet_to_not_run_node():
    with World() as world:
        node = world.create_init_node()

        from test_library.node import NodeIsNotRunning
        with pytest.raises(NodeIsNotRunning):
            node.attach_wallet()
