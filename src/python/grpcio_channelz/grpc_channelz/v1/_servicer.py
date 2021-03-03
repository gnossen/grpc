# Copyright 2020 The gRPC Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Channelz debug service implementation in gRPC Python."""

import grpc
from grpc._cython import cygrpc

import grpc_channelz.v1.channelz_pb2 as _channelz_pb2
import grpc_channelz.v1.channelz_pb2_grpc as _channelz_pb2_grpc

from google.protobuf import json_format

import ipaddress


def _byte_pack_address(address):
    if address.HasField("tcpip_address"):
        addr = address.tcpip_address.ip_address.decode('ascii')
        try:
            ip = ipaddress.IPv6Address(addr)
        except ipaddress.AddressValueError:
            ip = ipaddress.IPv4Address(addr)
        address.tcpip_address.ip_address = ip.packed


class ChannelzServicer(_channelz_pb2_grpc.ChannelzServicer):
    """Servicer handling RPCs for service statuses."""

    @staticmethod
    def GetTopChannels(request, context):
        try:
            return json_format.Parse(
                cygrpc.channelz_get_top_channels(request.start_channel_id),
                _channelz_pb2.GetTopChannelsResponse(),
            )
        except (ValueError, json_format.ParseError) as e:
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(str(e))

    @staticmethod
    def GetServers(request, context):
        try:
            return json_format.Parse(
                cygrpc.channelz_get_servers(request.start_server_id),
                _channelz_pb2.GetServersResponse(),
            )
        except (ValueError, json_format.ParseError) as e:
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(str(e))

    @staticmethod
    def GetServer(request, context):
        try:
            return json_format.Parse(
                cygrpc.channelz_get_server(request.server_id),
                _channelz_pb2.GetServerResponse(),
            )
        except ValueError as e:
            context.set_code(grpc.StatusCode.NOT_FOUND)
            context.set_details(str(e))
        except json_format.ParseError as e:
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(str(e))

    @staticmethod
    def GetServerSockets(request, context):
        try:
            return json_format.Parse(
                cygrpc.channelz_get_server_sockets(request.server_id,
                                                   request.start_socket_id,
                                                   request.max_results),
                _channelz_pb2.GetServerSocketsResponse(),
            )
        except ValueError as e:
            context.set_code(grpc.StatusCode.NOT_FOUND)
            context.set_details(str(e))
        except json_format.ParseError as e:
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(str(e))

    @staticmethod
    def GetChannel(request, context):
        try:
            return json_format.Parse(
                cygrpc.channelz_get_channel(request.channel_id),
                _channelz_pb2.GetChannelResponse(),
            )
        except ValueError as e:
            context.set_code(grpc.StatusCode.NOT_FOUND)
            context.set_details(str(e))
        except json_format.ParseError as e:
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(str(e))

    @staticmethod
    def GetSubchannel(request, context):
        try:
            return json_format.Parse(
                cygrpc.channelz_get_subchannel(request.subchannel_id),
                _channelz_pb2.GetSubchannelResponse(),
            )
        except ValueError as e:
            context.set_code(grpc.StatusCode.NOT_FOUND)
            context.set_details(str(e))
        except json_format.ParseError as e:
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(str(e))

    @staticmethod
    def GetSocket(request, context):
        try:
            response = json_format.Parse(
                cygrpc.channelz_get_socket(request.socket_id),
                _channelz_pb2.GetSocketResponse(),
            )
            # Core uses JSON for channelz and so does not byte-encode its
            # addresses. The proto prescribes byte packing, so we do that here.
            _byte_pack_address(response.socket.remote)
            _byte_pack_address(response.socket.local)

            return response
        except ValueError as e:
            context.set_code(grpc.StatusCode.NOT_FOUND)
            context.set_details(str(e))
        except json_format.ParseError as e:
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(str(e))
