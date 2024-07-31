FROM debian:bookworm-slim

RUN echo "deb http://deb.debian.org/debian/ unstable main" >> /etc/apt/sources.list
RUN apt update

#RUN apt install -y build-essential
RUN apt install -y g++
RUN apt install -y cmake
RUN apt install -y ninja-build
RUN apt install -y libboost-system-dev

WORKDIR /app
COPY . .

RUN cmake -B build
RUN cmake --build build

ENTRYPOINT ["./build/magicmirror"]
