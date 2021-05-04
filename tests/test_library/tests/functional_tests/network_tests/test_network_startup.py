from test_library import World


def test_network_startup():
    with World() as world:
        network = world.create_network()
        network.create_init_node()
        network.create_node()
        network.create_node()

        network.run()


def test_two_connected_networks_startup():
    with World() as world:
        first = world.create_network()
        first.create_init_node()
        for _ in range(3):
            first.create_node()

        second = world.create_network()
        for _ in range(3):
            second.create_node()

        first.connect_with(second)

        first.run()
        second.run()
