# Copyright 2020 The gRPC authors.
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
"""The Python asyncio implementation of the gRPC helloworld.Greeter server."""

import asyncio
import logging
# TODO: Shorten
import grpc.experimental.aio

grpc.experimental.aio.init_grpc_aio()

from helloworld_pb2 import HelloReply
from helloworld_pb2_grpc import GreeterServicer, add_GreeterServicer_to_server


class Greeter(GreeterServicer):

    # TODO: Type annotations.
    async def SayHello(self, request, context):
        return HelloReply(message=f"Hello {request.name}")


async def serve():
    servicer = Greeter()
    server = grpc.experimental.aio.server()
    add_GreeterServicer_to_server(servicer, server)
    server.add_insecure_port('localhost:50051')
    await server.start()
    await server.wait_for_termination()


if __name__ == '__main__':
    logging.basicConfig()
    asyncio.run(serve())
