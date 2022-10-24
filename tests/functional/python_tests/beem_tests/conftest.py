from typing import List, Protocol

import pytest

from beem import Hive


class NodeClientMaker(Protocol):
    def __call__(self, accounts: List[dict] = None) -> Hive:
        pass


@pytest.fixture
def node_client(node, worker_id) -> NodeClientMaker:
    def _node_client(accounts: List[dict] = None) -> Hive:
        accounts = accounts or []

        keys = node.config.private_key.copy()

        for account in accounts:
            keys.append(account["private_key"])

        node_url = f"http://{node.http_endpoint}"
        return Hive(node=node_url, no_broadcast=False, keys=keys, profile=worker_id, num_retries=-1)

    return _node_client
