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
"""The Python asyncio implementation of the gRPC helloworld.Greeter client."""

import asyncio
import logging
import grpc.experimental.aio as grpc_aio

from helloworld_pb2 import HelloRequest
from helloworld_pb2_grpc import GreeterStub


async def run_client():
    async with grpc_aio.insecure_channel('localhost:50051') as channel:
        stub = GreeterStub(channel)
        response = await stub.SayHello(HelloRequest(name="you"))
    print(f"Greeter client received: {response.message}")


if __name__ == "__main__":
    logging.basicConfig()
    asyncio.run(run_client())
