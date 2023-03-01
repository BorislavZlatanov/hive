import json

from typing import List

import test_tools as tt

class NodeWrapper:
    def __init__(self, name: str) -> None:
        self.name = name

class InitNodeWrapper(NodeWrapper):
    def __init__(self, name) -> None:
        super().__init__(name)

    def __str__(self) -> str:
        return f'({self.name})'

class ApiWrapper(NodeWrapper):
    def __init__(self, name) -> None:
        super().__init__(name)

    def __str__(self) -> str:
        return f'({self.name})'

class WitnessWrapper(NodeWrapper):
    def __init__(self, name) -> None:
        super().__init__(name)
        self.witnesses = []

    def create_witnesses(self, cnt_witness_start: int, witnesses_number: int, network_name: str):
        for i in range(cnt_witness_start, witnesses_number):
            self.witnesses.append( f"witness{i}-{network_name}" )

    def __str__(self) -> str:
        return f'({self.name} {"".join( "(" + witness_name + ")" for witness_name in self.witnesses)})'

class NetworkWrapper:
    def __init__(self, name) -> None:
        self.name           = name

        self.init_node      = None
        self.api_node       = None
        self.witness_nodes  = []

    def __str__(self) -> str:
        details = []
        details.append(f'({self.name})')

        if self.init_node is not None:
            details.append(str(self.init_node))

        if self.api_node is not None:
            details.append(str(self.api_node))

        for witness_node in self.witness_nodes:
            details.append(str(witness_node))

        return "\n  ".join( detail for detail in details)

class NetworksArchitecture:
    def __init__(self) -> None:
        self.networks       = []
        self.nodes_number   = 0

    def greek(self, idx: int) -> str:
        greek_alphabet = ['alpha',      'beta',     'gamma',    'delta',
                          'epsi',       'zeta',     'eta',      'theta',
                          'iota',       'kappa',    'lambda',   'mu',
                          'nu',         'xi',       'omi',      'pi',
                          'rho',        'sigma',    'tau',      'upsi',
                          'phi',        'chi',      'psi',      'omega' ]
        return greek_alphabet[idx % len(greek_alphabet)]

    def load(self, config: dict) -> None:
        assert 'networks' in config
        _networks = config['networks']

        cnt_network         = 0
        cnt_witness_start   = 0
        for network in _networks:
            current_net = NetworkWrapper(f'Network-{self.greek(cnt_network)}')

            if 'InitNode' in network:
                current_net.init_node = InitNodeWrapper('InitNode') if network['InitNode'] else None
                self.nodes_number += 1
            else:
                current_net.init_node = None

            if 'ApiNode' in network:
                current_net.api_node = ApiWrapper('ApiNode') if network['ApiNode'] else None
                self.nodes_number += 1
            else:
                current_net.api_node = None

            if 'WitnessNodes' in network:
                witness_nodes       = network['WitnessNodes']
                cnt_witness_node    = 0
                self.nodes_number += len(witness_nodes)
                for witnesses_number in witness_nodes:
                    current_witness = WitnessWrapper(f'WitnessNode-{cnt_witness_node}')
                    current_witness.create_witnesses(cnt_witness_start, cnt_witness_start + witnesses_number, self.greek(cnt_network))
                    cnt_witness_start += witnesses_number

                    current_net.witness_nodes.append(current_witness)

                    cnt_witness_node += 1
            else:
                current_net.witness_nodes = None

            self.networks.append(current_net)

            cnt_network += 1

    def __str__(self) -> str:
        return "\n".join(str(network) for network in self.networks)

class NetworksBuilder:
    def __init__(self) -> None:
        self.init_node      = None
        self.witness_names  = []
        self.networks       = []
        self.nodes          = []

    def build(self, architecture: NetworksArchitecture) -> None:
        for network in architecture.networks:
            tt_network = tt.Network()

            if network.init_node is not None:
                assert self.init_node == None, "InitNode already exists"
                self.init_node = tt.InitNode(network=tt_network)
                self.nodes.append(self.init_node)

            for witness in network.witness_nodes:
                self.witness_names.extend(witness.witnesses)
                self.nodes.append(tt.WitnessNode(witnesses=witness.witnesses, network=tt_network))

            if network.api_node is not None:
                self.nodes.append(tt.ApiNode(network=tt_network))

            self.networks.append(tt_network)

#=================Example=================

# config = {
#     "networks": [
#                     {
#                         "InitNode"     : True,
#                         "ApiNode"      : True,
#                         "WitnessNodes" :[ 1, 2, 4 ]
#                     },
#                     {
#                         "InitNode"     : False,
#                         "ApiNode"      : True,
#                         "WitnessNodes" :[ 6, 5 ]
#                     }
#                 ]
# }

# na = NetworksArchitecture()
# na.load(config)
# print(na)

#=================Result=================

# (Network-alpha)
#   (InitNode)
#   (ApiNode)
#   (WitnessNode-0 (witness0-alpha))
#   (WitnessNode-1 (witness1-alpha)(witness2-alpha))
#   (WitnessNode-2 (witness3-alpha)(witness4-alpha)(witness5-alpha)(witness6-alpha))
# (Network-beta)
#   (ApiNode)
#   (WitnessNode-0 (witness7-beta)(witness8-beta)(witness9-beta)(witness10-beta)(witness11-beta)(witness12-beta))
#   (WitnessNode-1 (witness13-beta)(witness14-beta)(witness15-beta)(witness16-beta)(witness17-beta))
