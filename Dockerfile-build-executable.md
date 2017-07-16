	FROM debian:stretch
	ENV DEBIAN_FRONTEND=stdin
	RUN apt-get update
	RUN echo -e "\nexport TERM=xterm\n" >> /root/.bashrc && \
	apt-get update && apt-get install -y \
	build-essential \
	qtbase5-dev \
	qtbase5-dev-tools \
	qt5-qmake \
	qt5-default \
	libtcmalloc-minimal4 \
	libexosip2-11 \
	curl \
	bash \
	libosip2-dev \
	libssl-dev \
	libexosip2-dev \
	libsystemd-dev \
	libqt5sql5-sqlite \
	make && \
	apt-get clean && \ 
	curl -L https://packages.gitlab.com/install/repositories/runner/gitlab-ci-multi-runner/script.deb.sh | bash && \
	echo -e "Explanation: Prefer GitLab provided packages over the Debian native ones\nPackage: gitlab-ci-multi-runner\nPin: origin packages.gitlab.com\nPin-Priority: 1001\n" > /etc/apt/preferences.d/pin-gitlab-runner.pref && \
	apt-get install -y gitlab-ci-multi-runner && \
	rm -rf /var/lib/apt/lists/* 
	RUN ln -s /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libtcmalloc_minimal.so
