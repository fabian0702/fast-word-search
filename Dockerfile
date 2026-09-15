FROM debian:trixie-slim AS builder

RUN apt update && apt upgrade -y
RUN apt install -y build-essential make

COPY src /build/src
COPY Makefile /build/

WORKDIR /build/

RUN make -j $(nproc)

FROM debian:trixie-slim AS runtime

WORKDIR /app/

COPY --from=builder /build/main /main

ENTRYPOINT [ "/main" ]