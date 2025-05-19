FROM ubuntu:latest

RUN apt update && apt install tzdata -y
ENV TZ="America/New_York"

RUN dpkg-reconfigure -f noninteractive tzdata
RUN apt -y install build-essential vim
RUN apt -y install libopencv-dev jq file
RUN apt -y install ffmpeg
