FROM gitpod/workspace-full

USER root

RUN apt-get -yq update \
	&& apt-get install -yq valgrind \
	&& apt-get install -y libsdl2-dev