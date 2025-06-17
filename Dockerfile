FROM debian:stable-slim as builder

COPY ./geodiff /tmp/geodiff

RUN apt update && apt install -y git cmake gcc g++ libsqlite3-dev libpq-dev

WORKDIR /tmp/geodiff

RUN mkdir build && \
    cd build && \
    cmake .. -DWITH_POSTGRESQL=TRUE && \
    make

FROM debian:stable-slim

RUN apt update && apt install -y libsqlite3-dev libpq-dev

COPY --from=builder /tmp/geodiff/build/geodiff /usr/bin/geodiff

RUN geodiff version
