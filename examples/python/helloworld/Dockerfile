FROM phusion/baseimage:latest

ENV SRV_DIR /srv/grpc

RUN mkdir -p "${SRV_DIR}"

RUN apt-get update -y && \
	apt-get install -y python3 python3-pip && \
	yes | pip3 install grpcio protobuf

COPY *.py "${SRV_DIR}/"

CMD ["/bin/bash", "-c", "python3 ${SRV_DIR}/greeter_server.py"]
