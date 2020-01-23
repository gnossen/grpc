# Copyright 2020, the gRPC authors.
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

import sys


def _uninstalled_protos(*args, **kwargs):
    raise NotImplementedError(
        "Install the protobuf package to use the protos function.")


def _uninstalled_services(*args, **kwargs):
    raise NotImplementedError(
        "Install the protobuf package to use the services function.")


def _uninstalled_protos_and_services(*args, **kwargs):
    raise NotImplementedError(
        "Install the protobuf package to use the protos_and_services function.")


def _interpreter_version_protos(*args, **kwargs):
    raise NotImplementedError(
        "The protos function is only on available on Python 3.X interpreters.")


def _interpreter_version_services(*args, **kwargs):
    raise NotImplementedError(
        "The services function is only on available on Python 3.X interpreters."
    )


def _interpreter_version_protos_and_services(*args, **kwargs):
    raise NotImplementedError(
        "The protos_and_services function is only on available on Python 3.X interpreters."
    )


if sys.version_info[0] < 3:
    protos = _interpreter_version_protos
    services = _interpreter_version_services
    protos_and_services = _interpreter_version_protos_and_services
else:
    try:
        from google import protobuf
    except (ModuleNotFoundError, ImportError) as e:
        # NOTE: It's possible that we're encountering a transitive ImportError, so
        # we check for that and re-raise if so.
        if "google" not in e.args[0]:
            raise e
        protos = _uninstalled_protos
        services = _uninstalled_services
        protos_and_services = _uninstalled_protos_and_services
    else:
        from google.protobuf import protos

        import contextlib
        import importlib
        import importlib.machinery
        import os

        from grpc import _service_reflection

        _PROTO_MODULE_SUFFIX = "_pb2_grpc"

        def _module_name_to_proto_file(module_name):
            components = module_name.split(".")
            proto_name = components[-1][:-1 * len(_PROTO_MODULE_SUFFIX)]
            return os.path.sep.join(components[:-1] + [proto_name + ".proto"])

        def _proto_file_to_module_name(proto_file):
            components = proto_file.split(os.path.sep)
            proto_base_name = os.path.splitext(components[-1])[0]
            return ".".join(
                components[:-1] + [proto_base_name + _PROTO_MODULE_SUFFIX])

        @contextlib.contextmanager
        def _augmented_syspath(new_paths):
            original_sys_path = sys.path
            if new_paths is not None:
                sys.path = sys.path + new_paths
            try:
                yield
            finally:
                sys.path = original_sys_path


        # NOTE(rbellevi): module_repr is an abstract method in Python 3.3 only,b
        #   but is still picked up by the linter.
        class ProtoLoader(importlib.abc.Loader):  # pylint: disable=abstract-method

            def __init__(self, module_name, protobuf_path):
                self._module_name = module_name
                self._protobuf_path = protobuf_path

            def create_module(self, spec):
                return None

            def exec_module(self, module):
                """Instantiate a module identical to the generated version.
                """
                # NOTE(rbellevi): include_paths are propagated via sys.path.
                proto_module = protos(self._protobuf_path)
                file_descriptor = getattr(proto_module,
                                          _service_reflection.DESCRIPTOR_KEY)
                for service_descriptor in file_descriptor.services_by_name.values():
                    _service_reflection.add_service_to_module(
                        module, service_descriptor)

        class ProtoFinder(importlib.abc.MetaPathFinder):

            def find_spec(self, fullname, path, target=None):  # pylint: disable=no-self-use
                del path
                del target
                filepath = _module_name_to_proto_file(fullname)
                for search_path in sys.path:
                    try:
                        prospective_path = os.path.join(search_path, filepath)
                        os.stat(prospective_path)
                    except (FileNotFoundError, NotADirectoryError):
                        continue
                    else:
                        return importlib.machinery.ModuleSpec(
                            fullname, ProtoLoader(fullname, filepath))

        def services(protobuf_path, *, include_paths=None):
            with _augmented_syspath(include_paths):
                module_name = _proto_file_to_module_name(protobuf_path)
                module = importlib.import_module(module_name)
                return module

        def protos_and_services(protobuf_path, *, include_paths=None):
            protos_ = protobuf.protos(protobuf_path, include_paths=include_paths)
            services_ = services(protobuf_path, include_paths=include_paths)
            return protos_, services_

        sys.meta_path.extend([ProtoFinder()])
