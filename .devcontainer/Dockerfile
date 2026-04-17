FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

COPY scripts/setup.sh /tmp/setup.sh

RUN bash /tmp/setup.sh --no-sudo && rm -f /tmp/setup.sh

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

USER $USERNAME

WORKDIR /workspaces/QuasarDB
